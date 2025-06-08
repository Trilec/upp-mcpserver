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

// -------------------- Implementation ----------------------------

namespace Upp {
namespace Ws {

inline String Frame::Encode(bool mask)
{
    String out;
    byte b0 = (fin ? 0x80 : 0x00) | (opcode & 0x0F);
    out.Cat(b0);

    int payload_len = payload.GetCount();
    byte b1 = mask ? 0x80 : 0x00;
    if(payload_len < 126) {
        out.Cat(b1 | (byte)payload_len);
    }
    else if(payload_len <= 0xFFFF) {
        out.Cat(b1 | 126);
        out.Cat((byte)(payload_len >> 8));
        out.Cat((byte)(payload_len & 0xFF));
    }
    else {
        out.Cat(b1 | 127);
        uint64 len64 = payload_len;
        for(int i = 7; i >= 0; --i)
            out.Cat((byte)(len64 >> (i * 8)));
    }

    String data = payload;
    if(mask) {
        for(int i = 0; i < 4; i++)
            mask_key[i] = (byte)Random();
        out.Cat((const char*)mask_key, 4);
        for(int i = 0; i < data.GetCount(); i++)
            data[i] ^= mask_key[i & 3];
    }
    out.Cat(data);
    return out;
}

inline bool Frame::Decode(const void* buf, int sz, int& used, bool expect_mask)
{
    used = 0;
    if(sz < 2)
        return false;

    const byte* b = static_cast<const byte*>(buf);
    fin = (b[0] & 0x80) != 0;
    opcode = b[0] & 0x0F;
    bool mask = (b[1] & 0x80) != 0;
    uint64 length = b[1] & 0x7F;
    int pos = 2;
    if(length == 126) {
        if(sz < pos + 2) return false;
        length = ((uint64)b[pos] << 8) | b[pos+1];
        pos += 2;
    } else if(length == 127) {
        if(sz < pos + 8) return false;
        length = 0;
        for(int i = 0; i < 8; i++)
            length = (length << 8) | b[pos++];
    }

    if(mask) {
        if(sz < pos + 4) return false;
        memcpy(mask_key, b + pos, 4);
        pos += 4;
    } else {
        memset(mask_key, 0, 4);
    }

    if(sz < pos + length)
        return false;

    payload = String((const char*)b + pos, (int)length);
    if(mask) {
        for(int i = 0; i < payload.GetCount(); i++)
            payload[i] ^= mask_key[i & 3];
    }

    len = (int)length;
    used = pos + (int)length;
    if(expect_mask && !mask)
        return false;
    return true;
}

inline void Endpoint::SendText(const String& s)
{
    Frame f;
    f.opcode = Frame::TEXT;
    f.payload = s;
    f.len = s.GetCount();
    SendFrame(f);
}

inline void Endpoint::SendBinary(const void* data, int len)
{
    Frame f;
    f.opcode = Frame::BINARY;
    f.payload = String((const char*)data, len);
    f.len = len;
    SendFrame(f);
}

inline void Endpoint::Close(int code, const String& reason)
{
    if(closed)
        return;
    Frame f;
    f.opcode = Frame::CLOSE;
    f.payload.Cat((byte)(code >> 8));
    f.payload.Cat((byte)(code & 0xFF));
    f.payload << reason;
    f.len = f.payload.GetCount();
    SendFrame(f);
    closed = true;
}

inline void Endpoint::SendFrame(Frame& f)
{
    String raw = f.Encode(masked);
    int n = raw.GetCount();
    outbuf.SetCount(outbuf.GetCount() + n);
    memcpy(~outbuf + outbuf.GetCount() - n, raw.Begin(), n);
    tx_bytes += n;
}

inline bool Endpoint::WritePending()
{
    if(outbuf.IsEmpty())
        return true;

    int n = sock.Put(~outbuf, outbuf.GetCount());
    if(n < 0) {
        Fatal(-1, "write error");
        return false;
    }
    if(n > 0) {
        outbuf.Remove(0, n);
    }
    return true;
}

inline bool Endpoint::ReadFrames()
{
    byte buffer[4096];
    int n = sock.Get(buffer, sizeof(buffer));
    if(n < 0) {
        Fatal(-1, "read error");
        return false;
    }
    if(n == 0)
        return true;

    rx_bytes += n;
    int old = inbuf.GetCount();
    inbuf.SetCount(old + n);
    memcpy(~inbuf + old, buffer, n);

    int used = 0;
    while(true) {
        Frame f;
        if(!f.Decode(~inbuf, inbuf.GetCount(), used, masked))
            break;
        inbuf.Remove(0, used);
        if(f.opcode == Frame::TEXT || f.opcode == Frame::BINARY)
        {
            if(f.opcode == Frame::TEXT)
                WhenText(f.payload);
            else
                WhenBinary(f.payload);
        }
        else
            HandleControl(f);
    }
    return true;
}

inline void Endpoint::HandleControl(Frame& f)
{
    switch(f.opcode) {
    case Frame::PING:
        {
            Frame pong;
            pong.opcode = Frame::PONG;
            pong.payload = f.payload;
            pong.len = pong.payload.GetCount();
            SendFrame(pong);
            break;
        }
    case Frame::CLOSE:
        closed = true;
        if(WhenClose)
            WhenClose(1000, f.payload.Mid(2));
        break;
    }
}

inline void Endpoint::Fatal(int err, const char* msg)
{
    closed = true;
    if(WhenError)
        WhenError(err);
}

inline bool Endpoint::HandshakeClient(const String& host, const String& path)
{
    String key;
    for(int i = 0; i < 16; i++)
        key.Cat((char)Random());
    String request;
    request << "GET " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "Sec-WebSocket-Key: " << Base64Encode(key) << "\r\n\r\n";

    if(!sock.PutAll(request))
        return false;

    String line, header;
    while(!(line = sock.GetLine(8192)).IsEmpty())
        header << line << "\n";

    if(header.Find("101") < 0)
        return false;

    masked = true;
    return true;
}

inline bool Endpoint::HandshakeServer(const HttpHeader&)
{
    String line, header;
    while(!(line = sock.GetLine(8192)).IsEmpty())
        header << line << "\n";

    String key;
    Vector<String> lines = Split(header, '\n');
    for(const String& l : lines) {
        if(ToLower(l).StartsWith("sec-websocket-key:")) {
            key = Trim(l.Mid(l.Find(':') + 1));
            break;
        }
    }
    if(key.IsEmpty())
        return false;

    byte sha1[20];
    SHA1(sha1, key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    String accept = Base64Encode((const char*)sha1, 20);

    String response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";

    if(!sock.PutAll(response))
        return false;

    masked = false;
    return true;
}

inline bool Endpoint::Pump()
{
    if(!WritePending())
        return false;
    if(!ReadFrames())
        return false;
    return true;
}

inline bool Server::Listen(uint16 port, const String& path, bool, const String&, const String&)
{
    ws_path = path;
    return listener.Listen(port);
}

inline void Server::Pump()
{
    TcpSocket s;
    while(listener.Accept(s)) {
        Ptr<Endpoint> ep = new Endpoint;
        ep->sock.Attach(s.GetSOCKET());
        if(ep->HandshakeServer(HttpHeader())) {
            clients.Add(ep);
            WhenAccept(*ep);
        }
        else
            delete ~ep;
    }

    for(int i = 0; i < clients.GetCount(); ) {
        Endpoint& c = *clients[i];
        if(!c.Pump() || c.IsClosed())
            clients.Remove(i);
        else
            ++i;
    }
}

inline bool Client::Connect(const String& url, bool)
{
    String u = url;
    bool ssl = u.StartsWith("wss://");
    if(u.StartsWith("ws://"))
        u = u.Mid(5);
    else if(u.StartsWith("wss://"))
        u = u.Mid(6);
    int slash = u.Find('/');
    String host = slash >= 0 ? u.Left(slash) : u;
    String path = slash >= 0 ? u.Mid(slash) : "/";

    int port = ssl ? 443 : 80;
    int colon = host.Find(':');
    if(colon >= 0) {
        port = ScanInt(host.Mid(colon+1));
        host = host.Left(colon);
    }

    if(!sock.Connect(host, port))
        return false;
    masked = true;
    return HandshakeClient(host, path);
}

} // namespace Ws
} // namespace Upp

