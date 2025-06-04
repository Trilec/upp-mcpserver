#pragma once
#include <Core/Core.h>
// No longer includes <WebSockets/WebSockets.h> from U++ standard library
// Instead, includes the new header-only WebSocket library
#include "../mcp_server_lib/WebSocket.h" // Relative path from include/ to mcp_server_lib/

// Forward declare if needed, though WebSocket.h should bring in Upp::Ws members
// namespace Upp { namespace Ws { class Server; class Endpoint; } }

using namespace Upp; // Keep Upp namespace for Core types

// Permissions, ToolFunc, ToolDefinition structs remain the same
struct Permissions {
    bool allowReadFiles=false; bool allowWriteFiles=false; bool allowDeleteFiles=false;
    bool allowRenameFiles=false; bool allowCreateDirs=false; bool allowSearchDirs=false;
    bool allowExec=false; bool allowNetworkAccess=false; bool allowExternalStorage=false;
    bool allowChangeAttributes=false; bool allowIPC=false;
    Permissions() = default;
};

using ToolFunc = std::function<Value(const JsonObject& args)>;

struct ToolDefinition {
    ToolFunc func;
    String description;
    JsonObject parameters;
};

// McpServer no longer inherits from Upp::WebSocketServer
class McpServer {
public:
    McpServer(int initial_port, const String& initial_path_prefix = "/mcp"); // Added path_prefix
    ~McpServer(); // Important for managing resources like active_clients if Ptr is not used or specific cleanup

    // Tool Management (largely unchanged)
    void AddTool(const String& toolName, const ToolDefinition& toolDef);
    Vector<String> GetAllToolNames() const;
    void EnableTool(const String& toolName);
    void DisableTool(const String& toolName);
    bool IsToolEnabled(const String& toolName) const;
    JsonObject GetToolManifest() const; // Returns the "tools" part of the manifest

    // Permissions & Sandbox (largely unchanged)
    Permissions& GetPermissions();
    const Permissions& GetPermissions() const;
    Vector<String>& GetSandboxRoots();
    const Vector<String>& GetSandboxRoots() const;
    void AddSandboxRoot(const String& root);
    void RemoveSandboxRoot(const String& root);
    void EnforceSandbox(const String& path) const;

    // Server Configuration & Control
    void ConfigureBind(bool allInterfaces); // true for 0.0.0.0, false for 127.0.0.1
    bool GetBindAllInterfaces() const { return bindAll; }

    void SetPort(uint16 port); // Changed to uint16 as per Ws::Server::Listen
    uint16 GetPort() const { return serverPort; }

    void SetPathPrefix(const String& path); // For the WebSocket path
    String GetPathPrefix() const { return ws_path_prefix; }

    // TLS configuration (placeholders, matching Ws::Server::Listen)
    void SetTls(bool use_tls, const String& cert_path = "", const String& key_path = "");

    bool StartServer();
    bool StopServer();
    bool IsListening() const { return is_listening; }

    void PumpEvents(); // New method to be called by main loop

    // Logging
    void SetLogCallback(std::function<void(const String&)> cb);
    void Log(const String& message); // Helper to use the logCallback

    // Utility (was for chaining, might not be needed if ws_server is private)
    // McpServer& GetServer() { return *this; }

    // Public member to access the log callback, IF NEEDED EXTERNALLY (e.g. Main.cpp directly).
    // Prefer using the Log() method internally.
    std::function<void(const String&)> logCallback;


private:
    // WebSocket Server instance (from new WebSocket.h)
    Upp::Ws::Server ws_server;

    // Configuration state
    uint16 serverPort;
    String ws_path_prefix; // e.g., "/mcp" or "/"
    bool bindAll;        // true for 0.0.0.0, false for 127.0.0.1
    bool use_tls = false;
    String tls_cert_path;
    String tls_key_path;

    bool is_listening = false;

    // Tool and permissions management state (as before)
    HashMap<String, ToolDefinition> allTools;
    HashSet<String> enabledTools;
    Permissions perms;
    Vector<String> sandboxRoots;

    // std::function<void(const String&)> logCallback; // Made public above

    // Active client management
    Index<Upp::Ws::Endpoint*> active_clients;

    // Callbacks for Upp::Ws::Server and Upp::Ws::Endpoint events
    void OnWsAccept(Upp::Ws::Endpoint& client_endpoint);
    void OnWsText(Upp::Ws::Endpoint* client_endpoint, String msg); // Changed from const String&
    void OnWsBinary(Upp::Ws::Endpoint* client_endpoint, String data); // Changed from const String&
    bool OnWsClientClose(Upp::Ws::Endpoint* client_endpoint, int code, const String& reason);
    void OnWsClientError(Upp::Ws::Endpoint* client_endpoint, int error_code);


    // Helpers
    static bool PathUnderRoot(const String& parent, const String& child);
    void SendJsonResponse(Upp::Ws::Endpoint* client, const JsonObject& obj);

    // Original ProcessMessage, now adapted for internal use with new WebSocket events
    void ProcessMcpMessage(Upp::Ws::Endpoint* client_endpoint, const JsonObject& msgObj);
};
