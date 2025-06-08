#pragma once
#include <Core/Core.h>
#include "../mcp_server_lib/WebSocket.h"

// Current application version
constexpr const char* MCP_SERVER_VERSION = "0.1.0";

using namespace Upp;

struct Permissions { /* ... remains same ... */
    bool allowReadFiles=false; bool allowWriteFiles=false; bool allowDeleteFiles=false;
    bool allowRenameFiles=false; bool allowCreateDirs=false; bool allowSearchDirs=false;
    bool allowExec=false; bool allowNetworkAccess=false; bool allowExternalStorage=false;
    bool allowChangeAttributes=false; bool allowIPC=false;
    Permissions() = default;
};

// ToolFunc now takes and returns Value. Args should be a ValueMap. Result can be any valid JSON Value.
using ToolFunc = std::function<Value(const Value& args)>; // Args Value is expected to be a ValueMap

struct ToolDefinition {
    ToolFunc func;
    String description;
    Value parameters; // Expected to be a ValueMap representing JSON schema object
};

class McpServer {
public:
    McpServer(int initial_port, const String& initial_path_prefix = "/mcp");
    ~McpServer();

    void AddTool(const String& toolName, const ToolDefinition& toolDef);
    Vector<String> GetAllToolNames() const;
    void EnableTool(const String& toolName);
    void DisableTool(const String& toolName);
    bool IsToolEnabled(const String& toolName) const;
    Value GetToolManifest() const; // Returns Value (a ValueMap for the "tools" object)

    Permissions& GetPermissions();
    const Permissions& GetPermissions() const;
    Vector<String>& GetSandboxRoots();
    const Vector<String>& GetSandboxRoots() const;
    void AddSandboxRoot(const String& root);
    void RemoveSandboxRoot(const String& root);
    void EnforceSandbox(const String& path) const;

    void ConfigureBind(bool allInterfaces);
    bool GetBindAllInterfaces() const { return bindAll; }
    void SetPort(uint16 port);
    uint16 GetPort() const { return serverPort; }
    void SetPathPrefix(const String& path);
    String GetPathPrefix() const { return ws_path_prefix; }
    void SetTls(bool use_tls, const String& cert_path = "", const String& key_path = "");

    bool StartServer();
    bool StopServer();
    bool IsListening() const { return is_listening; }
    void PumpEvents();
    void SetLogCallback(std::function<void(const String&)> cb);
    void Log(const String& message);

    std::function<void(const String&)> logCallback;

private:
    Upp::Ws::Server ws_server;
    uint16 serverPort; String ws_path_prefix;
    bool bindAll; bool use_tls = false; String tls_cert_path; String tls_key_path;
    bool is_listening = false;
    HashMap<String, ToolDefinition> allTools;
    HashSet<String> enabledTools;
    Permissions perms; Vector<String> sandboxRoots;
    Index<Upp::Ws::Endpoint*> active_clients;

    void OnWsAccept(Upp::Ws::Endpoint& client_endpoint);
    void OnWsText(Upp::Ws::Endpoint* client_endpoint, String msg);
    void OnWsBinary(Upp::Ws::Endpoint* client_endpoint, String data);
    bool OnWsClientClose(Upp::Ws::Endpoint* client_endpoint, int code, const String& reason);
    void OnWsClientError(Upp::Ws::Endpoint* client_endpoint, int error_code);

    static bool PathUnderRoot(const String& parent, const String& child);
    void SendJsonResponse(Upp::Ws::Endpoint* client, const Value& jsonData);
    void ProcessMcpMessage(Upp::Ws::Endpoint* client_endpoint, const String& message_text);
};
