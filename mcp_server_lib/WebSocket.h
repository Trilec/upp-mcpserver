// WebSocket/WebSocket.h (as provided by user, placed in mcp_server_lib/)
#pragma once
#include <Core/Core.h>
#include <Core/SSL/SSL.h>        // only used if TLS requested

namespace Upp { // Assuming Upp namespace was intended for Frame, Endpoint, Server, Client
namespace Ws {

// -------------------- low-level frame ----------------------------
struct Frame {
    enum OPC : byte { CONT=0, TEXT=1, BINARY=2, CLOSE=8, PING=9, PONG=0xA };
    bool  fin   = true;
    byte  opcode= TEXT;
    int   len   = 0;             // payload size
    byte  mask_key[4] = {0,0,0,0}; // Initialized
    String payload;

    String  Encode(bool mask);        // build raw bytes (mask=true for client)
    bool    Decode(const void* buf,int sz,int& used,bool expect_mask);
};

// -------------------- endpoint base ------------------------------
class Endpoint {
public:
    Event<String> WhenText;
    Event<String> WhenBinary;
    Gate2<int,const String&> WhenClose;   // return false to veto close
    Event<int>    WhenError; // Parameter is error code

    // outbound
    void  SendText(const String&);
    void  SendBinary(const void*,int);
    void  Close(int code=1000,const String&reason=""); // Added default for reason
    bool  IsClosed() const { return closed; }

    // must be called from owner loop
    bool  Pump();                     // returns false on fatal error

    // stats
    uint64  TxBytes() const { return tx_bytes; }
    uint64  RxBytes() const { return rx_bytes; }

    // Access to underlying socket for IP Address etc.
    const TcpSocket& GetSocket() const { return sock; } // Added for IP Addr

protected:
    TcpSocket sock;
    Buffer<byte> inbuf, outbuf; // Consider initializing size in constructor
    bool   masked  = false;           // true for client→server frames
    bool   closed  = false;
    Time   last_ping; // Should be initialized
    uint64 tx_bytes = 0; // Initialize
    uint64 rx_bytes = 0; // Initialize


    Endpoint() : last_ping(Time::Low()) {} // Constructor to init last_ping, potentially buffer sizes

    bool   HandshakeServer(const HttpHeader& h);
    bool   HandshakeClient(const String& host,const String& path);
    void   SendFrame(Frame&);
    bool   ReadFrames();
    bool   WritePending();
    void   HandleControl(Frame&);
    void   Fatal(int err,const char* msg); // Added const for msg

    // Make Endpoint non-copyable as it has TcpSocket member
    Endpoint(const Endpoint&) = delete;
    Endpoint& operator=(const Endpoint&) = delete;
};

// -------------------- server wrapper -----------------------------
class Server {
public:
    Server() = default; // Added default constructor

    bool  Listen(uint16 port,const String& path="/",
                 bool tls=false,const String& cert="",const String& key="");

    // user connects
    Event<Endpoint&> WhenAccept;

    // drive in owner loop
    void  Pump();            // accept new + pump all clients
    bool  IsFinished() const { return !listener.IsOpen(); } // Consider if listener is always valid
    int   ClientCount() const { return clients.GetCount(); }

    // Make Server non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

private:
    TcpSocket listener;
    Vector<Ptr<Endpoint>> clients; // Ptr<> implies heap allocated Endpoints managed by Server
    String  ws_path = "/";
};

// -------------------- client wrapper -----------------------------
class Client : public Endpoint { // Client inherits from Endpoint
public:
    Client() = default; // Added default constructor
    bool Connect(const String& url, bool tls=false);

    // Make Client non-copyable (implicitly non-copyable due to Endpoint base)
};

} // namespace Ws
} // namespace Upp
