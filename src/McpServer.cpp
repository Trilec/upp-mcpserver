#include "McpServer.h"
// Note: McpServer.h includes Core/Core.h and WebSockets/WebSockets.h, which pulls in Json.
// So direct includes of those might not be needed here if McpServer.h is comprehensive.

McpServer::McpServer(int port, int maxClients)
  : WebSocketServer(), // Base constructor
    maxClientsAllowed(maxClients), bindAll(false), serverPort(port), is_listening(false) {
    Port(port); MaxConnections(maxClientsAllowed);
    WhenAccept = THISBACK(OnNewConnection);
    // Initial log message should use the Log method to go through the callback if available
    // Log("McpServer initialized. Port: " + AsString(port)); // This call can't happen before logCallback is set.
    // It's better that Main.cpp logs this after setting the callback.
}

void McpServer::Log(const String& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        // Fallback for very early messages or if no callback is ever set.
        // RLOG is a U++ macro that typically logs to stderr or a debug output.
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
    // Stubbed for now. A real implementation would iterate `enabledTools`
    // and build the JSON object with descriptions and parameters from `allTools`.
    Log("GetToolManifest() called (stubbed).");
    JsonObject manifest;
    JsonObject toolsObj;
    for(const String& toolName : enabledTools) {
        const ToolDefinition* def = allTools.FindPtr(toolName);
        if(def) {
            JsonObject toolInfo;
            toolInfo("description", def->description);
            toolInfo("parameters", def->parameters); // Assuming parameters is already a JsonObject
            toolsObj(toolName, toolInfo);
        }
    }
    manifest("type", "manifest"); // As per protocol in design doc
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
    Log("EnforceSandbox called for path: " + path + " (minimal check).");
    if (sandboxRoots.IsEmpty()) {
        Log("  Sandbox not enforced: no roots defined. Access to '" + path + "' allowed by default in this mode.");
        return; // No roots means no enforcement as per earlier spec for server logic
    }
    String normalizedPath = NormalizePath(path);
    for (const String& r : sandboxRoots) {
        if (PathUnderRoot(r, normalizedPath)) {
            Log("  Path '" + normalizedPath + "' is within sandbox root '" + r + "'. Access GRANTED.");
            return;
        }
    }
    Log("  Path '" + normalizedPath + "' is OUTSIDE all sandbox roots. Access DENIED.");
    throw Exc("Sandbox violation: Access to path '" + normalizedPath + "' is denied.");
}

// Simplified PathUnderRoot for this phase. Robust version in earlier scaffolding.
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
    Port(port); // Call base class Port method if needed for subsequent Listen
    Log("Server port set to: " + AsString(serverPort));
}

String McpServer::GetListenHost() const {
    if (!IsListening()) return "N/A"; // Not applicable if not listening
    // This should ideally return the actual IP it's listening on.
    // WebSocketServer base class might have a method for this after Listen() succeeds.
    // For now, this is based on config.
    return bindAll ? "0.0.0.0" : "127.0.0.1";
}

// Minimal Start/Stop for GUI phase
bool McpServer::StartServer() {
    Log("Attempting to start server (minimal implementation)... Port: " + AsString(serverPort) + ", BindAll: " + AsString(bindAll));
    // This is where the actual U++ WebSocketServer::Listen() would be called.
    // We simulate it for now.
    // Port(serverPort); // Ensure port is set on WebSocketServer base
    // String hostToBind = bindAll ? "0.0.0.0" : "127.0.0.1";
    // if (!WebSocketServer::Listen(port, hostToBind)) { // Using 'port' member of WebSocketServer base
    //    Log("Minimal StartServer: WebSocketServer::Listen() FAILED for " + hostToBind + ":" + AsString(this->GetPort()));
    //    is_listening = false;
    //    return false;
    // }
    is_listening = true; // Simulate successful start
    Log("Minimal StartServer: Server is NOW LISTENING (simulated). Host: " + GetListenHost() + ", Port: " + AsString(GetPort()));
    return true;
}
bool McpServer::StopServer() {
    Log("Attempting to stop server (minimal implementation)...");
    // WebSocketServer::Close(); // Actual call in full backend implementation
    is_listening = false;
    Log("Minimal StopServer: Server is NOW STOPPED (simulated).");
    return true;
}

void McpServer::SetLogCallback(std::function<void(const String&)> cb) {
    logCallback = cb;
    Log("Log callback has been set for McpServer."); // Log this event using the new callback
}

// Stubs for WebSocket event handlers (will be fleshed out in backend phase)
// Ensure method signatures use WebSocket& as per U++ standard.
void McpServer::OnNewConnection(WebSocket& socket) {
    Log("New client connection (stub handler). IP: " + socket.GetPeerAddr());
    // Example: Send manifest on new connection
    // JsonObject manifest = GetToolManifest();
    // SendJson(socket, manifest);
    socket.Close("Stub connection: closing immediately."); // Close until fully implemented
}
void McpServer::ProcessMessage(WebSocket& socket, const JsonObject& msgObj) {
    Log("Processing message (stub handler) from " + socket.GetPeerAddr() + ": " + msgObj.ToString());
}
void McpServer::SendJson(WebSocket& socket, const JsonObject& obj) {
    Log("Sending JSON (stub handler) to " + socket.GetPeerAddr() + ": " + obj.ToString());
    socket.SendText(obj.ToString()); // Example: actually send it
}
