// WebSocket/WebSocket.h (as provided by user, placed in minimalclient/)
#pragma once
#include <Core/Core.h>
#include <Core/SSL/SSL.h>        // only used if TLS requested

namespace Upp {
namespace Ws {

struct Frame {
    enum OPC : byte { CONT=0, TEXT=1, BINARY=2, CLOSE=8, PING=9, PONG=0xA };
    bool  fin   = true; byte  opcode= TEXT; int   len   = 0; byte  mask_key[4] = {0,0,0,0}; String payload;
    String  Encode(bool mask); bool Decode(const void* buf,int sz,int& used,bool expect_mask);
};

class Endpoint {
public:
    Event<String> WhenText; Event<String> WhenBinary; Gate2<int,const String&> WhenClose; Event<int> WhenError;
    void  SendText(const String&); void  SendBinary(const void*,int); void  Close(int code=1000,const String&reason="");
    bool  IsClosed() const { return closed; }
    bool  Pump(); uint64  TxBytes() const { return tx_bytes; } uint64  RxBytes() const { return rx_bytes; }
    const TcpSocket& GetSocket() const { return sock; }
protected:
    TcpSocket sock; Buffer<byte> inbuf, outbuf; bool masked=false; bool closed=false; Time last_ping;
    uint64 tx_bytes=0; uint64 rx_bytes=0;
    Endpoint() : last_ping(Time::Low()) {}
    bool   HandshakeServer(const HttpHeader& h); bool   HandshakeClient(const String& host,const String& path);
    void   SendFrame(Frame&); bool   ReadFrames(); bool   WritePending(); void   HandleControl(Frame&);
    void   Fatal(int err,const char* msg);
    Endpoint(const Endpoint&) = delete; Endpoint& operator=(const Endpoint&) = delete;
};

class Server {
public:
    Server() = default;
    bool  Listen(uint16 port,const String& path="/", bool tls=false,const String& cert="",const String& key="");
    Event<Endpoint&> WhenAccept;
    void  Pump(); bool  IsFinished() const { return !listener.IsOpen(); } int ClientCount() const {return clients.GetCount();}
    Server(const Server&) = delete; Server& operator=(const Server&) = delete;
private:
    TcpSocket listener; Vector<Ptr<Endpoint>> clients; String  ws_path = "/";
};

class Client : public Endpoint {
public:
    Client() = default; bool Connect(const String& url, bool tls=false);
};

} // namespace Ws
} // namespace Upp
