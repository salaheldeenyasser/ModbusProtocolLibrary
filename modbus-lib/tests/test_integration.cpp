// Integration test: real ModbusClient <-> real ModbusServer
// via a thread-safe in-memory LoopTransport pair.

#include "../include/modbus/ModbusClient.h"
#include "../include/modbus/ModbusServer.h"
#include "../src/ModbusFrameCodec.h"
#include "./mocks/MockTransport.h"
#include <iostream>
#include <array>
#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

static int passed = 0;
static int failed = 0;
static void check(const char* n, bool c){
    if(c){ std::cout<<"  [PASS] "<<n<<"\n"; ++passed; }
    else  { std::cout<<"  [FAIL] "<<n<<"\n"; ++failed; }
}

// ── Blocking in-memory transport ──────────────────────────────────────────────
class LoopTransport : public IModbusTransport {
public:
    std::vector<u8>         buf_;
    std::mutex              mtx_;
    std::condition_variable cv_;
    LoopTransport*          peer_ = nullptr;

    void setPeer(LoopTransport* p){ peer_=p; }

    std::expected<size_t,ModbusError>
    send(std::span<const u8> data) override {
        { std::lock_guard lk(peer_->mtx_);
          peer_->buf_.assign(data.begin(),data.end()); }
        peer_->cv_.notify_one();
        return data.size();
    }
    std::expected<std::vector<u8>,ModbusError>
    receive(size_t, std::chrono::milliseconds timeout) override {
        std::unique_lock lk(mtx_);
        if(!cv_.wait_for(lk,timeout,[this]{return !buf_.empty();}))
            return std::unexpected(ModbusError(TransportErrorCode::Timeout));
        auto r=std::move(buf_); buf_.clear(); return r;
    }
    void flush() override { std::lock_guard lk(mtx_); buf_.clear(); }
    bool isOpen() const override { return true; }
    void close() override {}
};

// ── Rig: heap-owns server and client so they are never copied/moved ───────────
struct Rig {
    std::shared_ptr<RegisterMap>     regMap;
    std::unique_ptr<LoopTransport>   sT, cT;   // keep alive; server/client own raw ptrs
    std::unique_ptr<ModbusServer>    server;
    std::unique_ptr<ModbusClient>    client;

    explicit Rig(u16 nH=10, u16 nCoil=2000, u16 nI=10, u8 id=1)
        : regMap(std::make_shared<RegisterMap>(nCoil,nH,nI,2000))
        , sT(std::make_unique<LoopTransport>())
        , cT(std::make_unique<LoopTransport>())
    {
        sT->setPeer(cT.get());
        cT->setPeer(sT.get());
        // Transfer raw ownership to server/client; Rig keeps unique_ptr shells
        // but they are already nullptr after release() — transport lives inside
        // server/client.
        server = std::make_unique<ModbusServer>(
            std::unique_ptr<IModbusTransport>(sT.release()), regMap, id);
        client = std::make_unique<ModbusClient>(
            std::unique_ptr<IModbusTransport>(cT.release()));
        client->setDefaultTimeout(std::chrono::milliseconds(500));
        client->setRetryCount(1);
    }

    template<typename Op>
    auto pump(Op op){
        auto fut = std::async(std::launch::async, op);
        server->processOneRequest();
        return fut.get();
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────
void test_read_holding_registers(){
    std::cout<<"--- Integration: readHoldingRegisters ---\n";
    Rig rig;
    rig.regMap->writeHoldingRegister(0,1500);
    rig.regMap->writeHoldingRegister(1,253);
    rig.regMap->writeHoldingRegister(2,300);
    auto r=rig.pump([&]{return rig.client->readHoldingRegisters(1,0,3);});
    check("result has value",  r.has_value());
    check("3 values returned", r && r->size()==3);
    check("reg[0]=1500",       r && (*r)[0]==1500);
    check("reg[1]=253",        r && (*r)[1]==253);
    check("reg[2]=300",        r && (*r)[2]==300);
}
void test_write_single_register(){
    std::cout<<"\n--- Integration: writeSingleRegister ---\n";
    Rig rig;
    auto r=rig.pump([&]{return rig.client->writeSingleRegister(1,2,400);});
    check("write success",          r.has_value());
    check("register updated to 400",rig.regMap->readHoldingRegister(2)==400);
}
void test_write_single_coil(){
    std::cout<<"\n--- Integration: writeSingleCoil ---\n";
    Rig rig;
    auto r1=rig.pump([&]{return rig.client->writeSingleCoil(1,0,true);});
    check("coil ON success", r1.has_value());
    check("coil stored ON",  rig.regMap->readCoil(0)==true);
    auto r2=rig.pump([&]{return rig.client->writeSingleCoil(1,0,false);});
    check("coil OFF success",r2.has_value());
    check("coil stored OFF", rig.regMap->readCoil(0)==false);
}
void test_read_coils(){
    std::cout<<"\n--- Integration: readCoils ---\n";
    Rig rig;
    rig.regMap->writeCoil(0,true); rig.regMap->writeCoil(1,false); rig.regMap->writeCoil(2,true);
    auto r=rig.pump([&]{return rig.client->readCoils(1,0,3);});
    check("readCoils has value",r.has_value());
    check("coil[0]=ON",         r&&(*r)[0]==true);
    check("coil[1]=OFF",        r&&(*r)[1]==false);
    check("coil[2]=ON",         r&&(*r)[2]==true);
}
void test_write_multiple_registers(){
    std::cout<<"\n--- Integration: writeMultipleRegisters ---\n";
    Rig rig;
    std::array<u16,3> v={10,20,30};
    auto r=rig.pump([&]{
        return rig.client->writeMultipleRegisters(
            1,0,std::span<const u16>(v.data(),v.size()));
    });
    check("write multiple success",r.has_value());
    check("reg[0]=10", rig.regMap->readHoldingRegister(0)==10);
    check("reg[1]=20", rig.regMap->readHoldingRegister(1)==20);
    check("reg[2]=30", rig.regMap->readHoldingRegister(2)==30);
}
void test_exception_propagation(){
    std::cout<<"\n--- Integration: exception propagation ---\n";
    Rig rig(5,2000,5);
    auto r=rig.pump([&]{return rig.client->readHoldingRegisters(1,100,1);});
    check("exception - returns error",!r.has_value());
    if(!r.has_value()){
        bool isEx=std::holds_alternative<ModbusException>(r.error());
        check("exception - ModbusException",isEx);
        if(isEx){
            auto& ex=std::get<ModbusException>(r.error());
            check("exception - IllegalDataAddress",
                  ex.exceptionCode==static_cast<u8>(ExceptionCode::IllegalDataAddress));
        }
    }
}
void test_write_callback_fires(){
    std::cout<<"\n--- Integration: write callback ---\n";
    Rig rig;
    u16 cbA=0,cbV=0; bool fired=false;
    rig.regMap->onHoldingRegisterWrite(2,[&](u16 a,u16 v){fired=true;cbA=a;cbV=v;});
    auto r=rig.pump([&]{return rig.client->writeSingleRegister(1,2,999);});
    check("write success",    r.has_value());
    check("callback fired",   fired);
    check("callback addr=2",  cbA==2);
    check("callback val=999", cbV==999);
}
void test_read_input_registers(){
    std::cout<<"\n--- Integration: readInputRegisters ---\n";
    Rig rig(10,2000,10);
    rig.regMap->writeInputRegister(0,4800);
    rig.regMap->writeInputRegister(1,236);
    auto r=rig.pump([&]{return rig.client->readInputRegisters(1,0,2);});
    check("readInputRegs has value",r.has_value());
    check("inputReg[0]=4800",       r&&(*r)[0]==4800);
    check("inputReg[1]=236",        r&&(*r)[1]==236);
}
void test_illegal_function_code(){
    std::cout<<"\n--- Integration: illegal function code ---\n";
    auto* mock=new MockTransport();
    ModbusServer server(std::unique_ptr<IModbusTransport>(mock),
                        std::make_shared<RegisterMap>(),1);
    ModbusFrame f; f.slaveID=1; f.functionCode=0x07; f.data={0x00,0x00};
    mock->injectResponse(ModbusFrameCodec::encodeRtuRequest(f));
    mock->clearSent();
    server.processOneRequest();
    const auto& sent=mock->lastSentBytes();
    check("illegal FC - responded",   !sent.empty());
    check("illegal FC - 0x87",        sent.size()>=2&&sent[1]==0x87);
    check("illegal FC - code 0x01",   sent.size()>=3&&sent[2]==0x01);
}

int main(){
    std::cout<<"=== Integration Tests (Client <-> Server) ===\n\n";
    test_read_holding_registers();
    test_write_single_register();
    test_write_single_coil();
    test_read_coils();
    test_write_multiple_registers();
    test_exception_propagation();
    test_write_callback_fires();
    test_read_input_registers();
    test_illegal_function_code();
    std::cout<<"\n"<<passed<<" passed, "<<failed<<" failed.\n";
    return (failed==0)?0:1;
}
