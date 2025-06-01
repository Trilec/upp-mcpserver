#pragma once
#include <Core/Core.h>
#include <WebSockets/WebSockets.h> // Includes Json/Json.h for JsonObject, Value

using namespace Upp;

struct Permissions {
    bool allowReadFiles=false; bool allowWriteFiles=false; bool allowDeleteFiles=false;
    bool allowRenameFiles=false; bool allowCreateDirs=false; bool allowSearchDirs=false;
    bool allowExec=false; bool allowNetworkAccess=false; bool allowExternalStorage=false;
    bool allowChangeAttributes=false; bool allowIPC=false;

    // Default constructor for easy initialization
    Permissions() = default;
};

using ToolFunc = std::function<Value(const JsonObject& args)>; // U++ uses Value for JsonValue

struct ToolDefinition {
    ToolFunc func;
    String description;
    JsonObject parameters;
};

class McpServer : public WebSocketServer {
public:
    McpServer(int port, int maxClients = 10);

    void AddTool(const String& toolName, const ToolDefinition& toolDef);
    Vector<String> GetAllToolNames() const;
    void EnableTool(const String& toolName);
    void DisableTool(const String& toolName);
    bool IsToolEnabled(const String& toolName) const;
    JsonObject GetToolManifest() const;

    Permissions& GetPermissions(); // Non-const for modification by GUI/Config
    const Permissions& GetPermissions() const; // Const for read-only access

    Vector<String>& GetSandboxRoots(); // Non-const for modification by GUI/Config
    const Vector<String>& GetSandboxRoots() const;
    void AddSandboxRoot(const String& root); // Already present
    void RemoveSandboxRoot(const String& root); // Already present
    void EnforceSandbox(const String& path) const;

    void ConfigureBind(bool allInterfaces);
    bool GetBindAllInterfaces() const { return bindAll; } // Added for status display

    bool StartServer(); // Will be minimal for now
    bool StopServer();  // Will be minimal for now
    bool IsListening() const { return is_listening; } // Added

    void SetPort(int port);
    int GetPort() const { return serverPort; } // Added for status display
    String GetListenHost() const; // Added for status display (can be stubbed)

    void SetLogCallback(std::function<void(const String&)> cb);
    // Helper for McpServer to log its own messages via the callback
    void Log(const String& message);

    McpServer& GetServer() { return *this; } // For chaining or direct access

    // Public member to access the log callback, needed by McpApplication for tool registration log
    std::function<void(const String&)> logCallback;


private:
    HashMap<String, ToolDefinition> allTools;
    HashSet<String> enabledTools;
    Permissions perms;
    Vector<String> sandboxRoots;
    int maxClientsAllowed;
    bool bindAll;
    int serverPort;
    // std::function<void(const String&)> logCallback; // Made public
    bool is_listening = false; // Added state for IsListening, Start/StopServer

    void OnNewConnection(WebSocket& socket); // Changed WebSocketSocket& to WebSocket&
    void ProcessMessage(WebSocket& socket, const JsonObject& msgObj); // Changed WebSocketSocket& to WebSocket&
    void SendJson(WebSocket& socket, const JsonObject& obj); // Changed WebSocketSocket& to WebSocket&
    static bool PathUnderRoot(const String& parent, const String& child);
};
