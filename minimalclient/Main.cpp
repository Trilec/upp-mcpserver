#include "WebSocket.h" // Local copy in this package
#include <Core/Core.h>
#include <Console/Console.h> // For any console interaction if needed (not used much here)

using namespace Upp;
using namespace Upp::Ws; // Assuming classes are in Upp::Ws

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT | LOG_TIMESTAMP);
    SetExitCode(0);
    LOG("Minimal WebSocket Client (Upp::Ws) starting...");

    Client ws_client; // Using Upp::Ws::Client
    bool received_welcome = false;
    bool server_initiated_close = false;

    ws_client.WhenText = [&](String s) {
        LOG("Client Received: " + s);
        if (s.StartsWith("Welcome from MinimalWsServer")) {
            received_welcome = true;
        }
    };

    ws_client.WhenClose = [&](int code, const String& reason) {
        LOG("Client: Connection closed by server. Code: " + AsString(code) + ", Reason: '" + reason + "'");
        server_initiated_close = true; // Mark that server closed it
    };

    ws_client.WhenError = [&](int err_code) {
        LOG("Client: WebSocket error. Code: " + AsString(err_code) + ", System: " + GetLastSystemError());
    };

    String url = "ws://localhost:9002/test";
    LOG("Client: Attempting to connect to " + url);

    if (ws_client.Connect(url)) {
        LOG("Client: Connected successfully to " + url);

        TimeStop connect_timeout;
        connect_timeout.Set(2000);
        while(!ws_client.IsClosed() && !received_welcome && !connect_timeout.IsTimeOut()) {
            ws_client.Pump();
            Sleep(10);
        }

        if (!received_welcome && !ws_client.IsClosed()) {
            LOG("Client: Did not receive welcome message from server in time.");
        } else if(received_welcome) {
            LOG("Client: Welcome message received!");
        } else if (ws_client.IsClosed()) {
            LOG("Client: Connection closed during welcome wait.");
        }


        for (int i = 0; i < 3; i++) {
            if (ws_client.IsClosed()) {
                LOG("Client: Connection closed before sending message " + AsString(i+1));
                break;
            }
            String msg_to_send = "Hello from U++ Ws::Client, message #" + AsString(i + 1);
            LOG("Client Sending: " + msg_to_send);
            if (!ws_client.SendText(msg_to_send)) {
                LOG("Client: Failed to send message: " + msg_to_send);
                break;
            }

            TimeStop reply_timeout;
            reply_timeout.Set(1000);
            while(!ws_client.IsClosed() && !reply_timeout.IsTimeOut()) {
                ws_client.Pump();
                Sleep(10);
            }
        }

        if (!ws_client.IsClosed()) {
            LOG("Client: Done sending messages. Closing connection.");
            ws_client.Close(1000, "Client session finished");
        }

        TimeStop close_ack_timeout;
        close_ack_timeout.Set(2000);
        while(!ws_client.IsClosed() && !server_initiated_close && !close_ack_timeout.IsTimeOut()) {
            ws_client.Pump(); // Pump to process potential close frame from server
            Sleep(10);
        }

        if (!ws_client.IsClosed() && !server_initiated_close) {
             LOG("Client: Connection not fully closed by server after client initiated close and timeout. Local cleanup will occur.");
        } else if (server_initiated_close) {
             LOG("Client: Server acknowledged close or closed connection.");
        } else {
             LOG("Client: Connection appears closed locally after timeout.");
        }

    } else {
        LOG("Client: Connection to " + url + " FAILED. System Error: " + GetLastSystemError());
        SetExitCode(1);
    }

    LOG("Minimal WebSocket Client finished.");
}
