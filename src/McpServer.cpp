#include "McpServer.h"

McpServer::McpServer(int port, int maxClients)
  : WebSocketServer(),
    maxClientsAllowed(maxClients), bindAll(false), serverPort(port), is_listening(false) {
    Port(port); MaxConnections(maxClientsAllowed);
    WhenAccept = THISBACK(OnNewConnection);
    // Log("McpServer initialized. Port: " + AsString(port)); // Initial log done after callback set in Main
}

void McpServer::Log(const String& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        RLOG("McpServer Log (no callback): " + message);
    }
}

void McpServer::AddTool(const String& toolName, const ToolDefinition& toolDef) {
    allTools.GetAdd(toolName) = toolDef;
    Log("Tool added: " + toolName);
}
Vector<String> McpServer::GetAllToolNames() const { return allTools.GetKeys(); }

void McpServer::EnableTool(const String& toolName) {
    if (allTools.FindPtr(toolName)) {
        enabledTools.Add(toolName);
        Log("Tool enabled: " + toolName);
    } else {
        Log("Warning: Attempt to enable non-existent tool: " + toolName);
    }
}
void McpServer::DisableTool(const String& toolName) {
    enabledTools.RemoveKey(toolName);
    Log("Tool disabled: " + toolName);
}
bool McpServer::IsToolEnabled(const String& toolName) const { return enabledTools.Find(toolName) >= 0; }

JsonObject McpServer::GetToolManifest() const {
    Log("GetToolManifest() called.");
    JsonObject manifest;
    JsonObject toolsObj;
    for(const String& toolName : enabledTools) {
        const ToolDefinition* def = allTools.FindPtr(toolName);
        if(def) {
            JsonObject toolInfo;
            toolInfo("description", def->description);
            toolInfo("parameters", def->parameters);
            toolsObj(toolName, toolInfo);
        }
    }
    manifest("type", "manifest");
    manifest("tools", toolsObj);
    return manifest;
}
Permissions& McpServer::GetPermissions() { return perms; }
const Permissions& McpServer::GetPermissions() const { return perms; }
Vector<String>& McpServer::GetSandboxRoots() { return sandboxRoots; }
const Vector<String>& McpServer::GetSandboxRoots() const { return sandboxRoots; }

void McpServer::AddSandboxRoot(const String& root) {
    String normRoot = NormalizePath(root);
    if(normRoot.IsEmpty()) return;
    if(sandboxRoots.Find(normRoot) < 0) {
        sandboxRoots.Add(normRoot);
        Log("Sandbox root added: " + normRoot);
    }
}
void McpServer::RemoveSandboxRoot(const String& root) {
    String normRoot = NormalizePath(root);
    if(sandboxRoots.RemoveKey(normRoot)) {
        Log("Sandbox root removed: " + normRoot);
    }
}

void McpServer::EnforceSandbox(const String& path) const {
    // Log("EnforceSandbox called for path: " + path); // Too verbose for every call
    if (sandboxRoots.IsEmpty()) {
        Log("Warning: EnforceSandbox active but no sandbox roots are defined. Path '" + path + "' allowed by default in this scenario.");
        return;
    }
    String normalizedPath = NormalizePath(path);
    for (const String& r : sandboxRoots) {
        if (PathUnderRoot(r, normalizedPath)) {
            // Log("Path '" + normalizedPath + "' is within sandbox root '" + r + "'. Access GRANTED."); // Verbose
            return;
        }
    }
    Log("Sandbox violation: Path '" + normalizedPath + "' is outside defined sandbox roots. Access DENIED.");
    throw Exc("Sandbox violation: Path '" + normalizedPath + "' is outside defined sandbox roots.");
}

bool McpServer::PathUnderRoot(const String& parent, const String& child) {
    String nParent = NormalizePath(parent);
    String nChild = NormalizePath(child);
    if (nChild == nParent) return true;
    String parentPrefix = nParent;
    if (parentPrefix.GetCount() > 0 && parentPrefix.Last() != DIR_SEPARATOR && parentPrefix.Last() != '\\' && parentPrefix.Last() != '/') {
        parentPrefix.Cat(DIR_SEPARATOR);
    }
    return nChild.StartsWith(parentPrefix);
}

void McpServer::ConfigureBind(bool all) {
    bindAll = all;
    Log("Bind all interfaces set to: " + AsString(bindAll));
}
void McpServer::SetPort(int port) {
    serverPort = port;
    Port(port);
    Log("Server port set to: " + AsString(serverPort));
}

String McpServer::GetListenHost() const {
    if (!IsListening()) return "N/A";
    return bindAll ? "0.0.0.0" : "127.0.0.1";
}

bool McpServer::StartServer() {
    Log("Attempting to start server (minimal implementation)... Port: " + AsString(serverPort) + ", BindAll: " + AsString(bindAll));
    is_listening = true;
    Log("Minimal StartServer: Server is NOW LISTENING (simulated). Host: " + GetListenHost() + ", Port: " + AsString(GetPort()));
    return true;
}
bool McpServer::StopServer() {
    Log("Attempting to stop server (minimal implementation)...");
    is_listening = false;
    Log("Minimal StopServer: Server is NOW STOPPED (simulated).");
    return true;
}

void McpServer::SetLogCallback(std::function<void(const String&)> cb) {
    logCallback = cb;
    // Log("Log callback has been set for McpServer."); // This can be logged by caller (Main.cpp)
}

void McpServer::OnNewConnection(WebSocket& socket) {
    Log("New client connection (stub handler). IP: " + socket.GetPeerAddr());
    socket.Close("Stub connection: closing immediately.");
}
void McpServer::ProcessMessage(WebSocket& socket, const JsonObject& msgObj) {
    Log("Processing message (stub handler) from " + socket.GetPeerAddr() + ": " + msgObj.ToString());
}
void McpServer::SendJson(WebSocket& socket, const JsonObject& obj) {
    Log("Sending JSON (stub handler) to " + socket.GetPeerAddr() + ": " + obj.ToString());
    socket.SendText(obj.ToString());
}
