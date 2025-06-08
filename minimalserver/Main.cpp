#include "WebSocket.h" // Local copy in this package
#include <Core/Core.h>
#include <Core/SSL/SSL.h> // For SSLinit, if server were to use TLS immediately

using namespace Upp;
using namespace Upp::Ws; // Assuming classes are in Upp::Ws

CONSOLE_APP_MAIN {
    // Call this once for SSL if TLS were to be used in Server::Listen
    // SSLinit(); // Or SSLinit_nossl if no server-side SSL needed by U++ Core globally

    StdLogSetup(LOG_COUT | LOG_TIMESTAMP);
    LOG("Minimal WebSocket Server (Upp::Ws) starting...");

    Server hub; // Using Upp::Ws::Server

    hub.WhenAccept = [&](Endpoint& cli) {
        String client_ip = cli.GetSocket().GetPeerAddr();
        LOG("Client accepted: " + client_ip);

        cli.WhenText = [&](String m) {
            LOG("Received from client (" + client_ip + "): " + m);
            String response = "Server echoes: " + m;
            LOG("Sending to client (" + client_ip + "): " + response);
            if(!cli.SendText(response)) { // Check send result
                LOG("Failed to send echo to client: " + client_ip);
            }
        };

        cli.WhenClose = [=](int code, const String& reason) mutable { // Capture client_ip by value
            LOG("Client (" + client_ip +") disconnected. Code: " + AsString(code) + ", Reason: '" + reason + "'");
        };

        cli.WhenError = [=](int err_code) mutable { // Capture client_ip by value
            LOG("WebSocket error for client (" + client_ip + "). Code: " + AsString(err_code) + ", System: " + GetLastSystemError());
        };

        cli.SendText("Welcome from MinimalWsServer (Upp::Ws)!");
    };

    uint16 port = 9002;
    String path = "/test";
    if (hub.Listen(port, path)) {
        LOG("Listening on port " + AsString(port) + ", path " + path);
        LOG("Server running. Press Ctrl+C to exit.");
        while (true) {
            hub.Pump();
            Ctrl::ProcessEvents();
            Sleep(1);
        }
    } else {
        LOG("Failed to listen on port " + AsString(port) + ". System Error: " + GetLastSystemError());
    }
    LOG("Minimal WebSocket Server finished.");
}
