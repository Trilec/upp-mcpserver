#pragma once
#include <Core/Core.h>
#include <WebSockets/WebSockets.h>

using namespace Upp;

/**
 * @brief Permission flags to control what AI tools can do.
 */
struct Permissions {
    bool allowReadFiles       = false;  ///< Read file contents
    bool allowWriteFiles      = false;  ///< Create/overwrite files
    bool allowDeleteFiles     = false;  ///< Delete files
    bool allowRenameFiles     = false;  ///< Rename/move files
    bool allowCreateDirs      = false;  ///< Create directories
    bool allowSearchDirs      = false;  ///< List/search folders
    bool allowExec            = false;  ///< Execute external processes
    bool allowNetworkAccess   = false;  ///< Network calls (HTTP, sockets)
    bool allowExternalStorage = false;  ///< Access removable drives
    bool allowChangeAttributes= false;  ///< Modify file timestamps/attributes
    bool allowIPC             = false;  ///< Open named pipes / local sockets
};

/**
 * @brief Signature for a tool’s callback function.
 *        Receives arguments as JSON and returns result as JSON.
 */
using ToolFunc = std::function<JsonValue(const JsonObject& args)>;

/**
 * @brief Metadata for a registered tool (name, description, parameters).
 */
struct ToolDefinition {
    ToolFunc func;             ///< The function to call
    String description;        ///< Short sentence about what the tool does
    JsonObject parameters;     ///< JSON schema for arguments
};

/**
 * @brief Core MCP Server: WebSocket server that manages tools, permissions, and sandboxing.
 */
class McpServer : public WebSocketServer {
public:
    /**
     * @param port         The TCP port for WebSocket server (e.g., 5000).
     * @param maxClients   Maximum concurrent WebSocket clients.
     */
    McpServer(int port, int maxClients = 10);

    /** Register a tool (plugin) by name and definition. */
    void            AddTool(const String& toolName, const ToolDefinition& toolDef);

    /** Return a list of all registered tool names (for GUI listing). */
    Vector<String>  GetAllToolNames() const;

    /** Enable a registered tool so that it appears in the manifest. */
    void            EnableTool(const String& toolName);

    /** Disable a tool (hide from manifest). */
    void            DisableTool(const String& toolName);

    /** Check if a tool is currently enabled. */
    bool            IsToolEnabled(const String& toolName) const;

    /** Build a JSON manifest of all enabled tools (name→metadata). */
    JsonObject      GetToolManifest() const;

    /** Access or modify permission flags. */
    Permissions&         GetPermissions();
    const Permissions&   GetPermissions() const;

    /** Add or remove a root directory for sandboxing. */
    void            AddSandboxRoot(const String& root);
    void            RemoveSandboxRoot(const String& root);
    const Vector<String>& GetSandboxRoots() const;

    /** Throw if `path` is outside all defined sandbox roots. */
    void            EnforceSandbox(const String& path) const;

    /** Specify whether to bind to “0.0.0.0” (all interfaces) or just “127.0.0.1”. */
    void            ConfigureBind(bool allInterfaces);

    /** Start listening on WebSocket; returns true if bound successfully. */
    bool            StartServer();

    /** Stop listening and disconnect all clients. */
    void            StopServer();

    /** Set the TCP port (before StartServer). */
    void            SetPort(int port);

    /** Supply a callback to receive log messages (for GUI or file). */
    void            SetLogCallback(std::function<void(const String&)> cb);

private:
    HashMap<String, ToolDefinition> allTools;      ///< All registered tools
    HashSet<String>                 enabledTools;  ///< Currently enabled tool names
    Permissions                     perms;         ///< Current permission flags
    Vector<String>                  sandboxRoots;  ///< Allowed directories for file I/O
    int                             maxClientsAllowed;
    bool                            bindAll;       ///< Bind to 0.0.0.0 if true
    int                             serverPort;    ///< The configured port number
    std::function<void(const String&)> logCallback;///< To emit log messages

    /** Handle a new WebSocket connection: send manifest, bind receive handler. */
    void            OnNewConnection(WebSocketSocket& socket);

    /** Process an incoming JSON message (tool_call, etc.). */
    void            ProcessMessage(WebSocketSocket& socket, const JsonObject& msgObj);

    /** Send a JSON object over the socket. */
    void            SendJson(WebSocketSocket& socket, const JsonObject& obj);

    /** Internal helper: is `child` under `parent` (path matching). */
    static bool     PathUnderRoot(const String& parent, const String& child);
};
