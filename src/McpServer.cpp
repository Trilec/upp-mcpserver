#include "McpServer.h"
// Assuming Core.h and Json.h are included appropriately by McpServer.h or its dependencies,
// or that U++ build system makes them available.

/**
 * @file McpServer.cpp
 * @brief Core MCP Server implementation: WebSocket handling, tool management, sandboxing.
 */

/*
  Constructor:
    - Call WebSocketServer(port)
    - Set internal `serverPort = port` and `maxClientsAllowed`
    - Bind Accept(...) so that new connections invoke OnNewConnection().
*/
McpServer::McpServer(int port, int maxClients)
  : WebSocketServer(port), // This needs to be configured if WebSocketServer itself needs port in constructor
    maxClientsAllowed(maxClients),
    bindAll(false),
    serverPort(port)
{
    // The base WebSocketServer class in U++ might not take port in constructor directly.
    // It might be SetPort then Listen. Or Port(port_num) then Listen.
    // For now, following pseudocode structure. U++ specific init might differ.
    // Typically, it's:
    // Port(port).MaxConnections(maxClients);
    // WhenAccept = THISBACK(OnNewConnection);

    Port(port); // Assuming U++ WebSocketServer has a Port() method
    MaxConnections(maxClientsAllowed); // Assuming U++ WebSocketServer has MaxConnections()
    WhenAccept = [this](WebSocket& socket){ // U++ uses WebSocket& not WebSocketSocket&
        OnNewConnection(socket);
    };
    // The pseudocode used WebSocketSocket&, U++ docs usually show WebSocket&
    // And Accept() usually takes a callback directly or uses WhenAccept member.
    // The original pseudocode: Accept([&](WebSocketSocket& sock){ OnNewConnection(sock); });
    // Let's stick to the lambda signature from the original pseudocode for now,
    // but note U++ specifics might require WebSocket&.
    // The header defined OnNewConnection with WebSocketSocket&, so let's keep that for consistency with header.
    // WhenAccept = [this](WebSocketSocket& sock){ OnNewConnection(sock); };
    // This is tricky: U++ WebSocketServer provides `WebSocket&` to `WhenAccept`.
    // The header defined `OnNewConnection(WebSocketSocket& socket);`
    // This implies WebSocketSocket is either a typedef or a specific class.
    // Given WebSockets/WebSockets.h, it's likely `WebSocket&`.
    // For now, this will cause a type mismatch if WebSocketSocket is not WebSocket.
    // Let's assume WebSocketSocket is the correct type as per header for now.
    // If WebSocketSocket is indeed a distinct type from UPP's WebSocket, this is fine.
    // If it's meant to be UPP's WebSocket, the header and this .cpp needs to change.
    // The design doc uses WebSocketSocket in McpServer.h, so we follow that.
    WhenAccept = [this](WebSocketSocket& sock){ this->OnNewConnection(sock); };


    if(logCallback) logCallback("McpServer initialized on port " + AsString(serverPort));
}

/*
  AddTool:
    - allTools[toolName] = toolDef
*/
void McpServer::AddTool(const String& toolName, const ToolDefinition& toolDef) {
    allTools.GetAdd(toolName) = toolDef;
    if(logCallback) logCallback("Tool added: " + toolName);
}

/*
  GetAllToolNames:
    - Return vector of keys from allTools
*/
Vector<String> McpServer::GetAllToolNames() const {
    return allTools.GetKeys(); // More direct way to get keys in U++
}

/*
  EnableTool / DisableTool / IsToolEnabled:
    - Manage the `enabledTools` set
*/
void McpServer::EnableTool(const String& toolName) {
    if (allTools.FindPtr(toolName)) { // Check if tool exists before enabling
        enabledTools.Add(toolName);
        if(logCallback) logCallback("Tool enabled: " + toolName);
    } else {
        if(logCallback) logCallback("Attempted to enable non-existent tool: " + toolName);
    }
}
void McpServer::DisableTool(const String& toolName) {
    enabledTools.RemoveKey(toolName);
    if(logCallback) logCallback("Tool disabled: " + toolName);
}
bool McpServer::IsToolEnabled(const String& toolName) const {
    return enabledTools.Find(toolName) >= 0;
}

/*
  GetToolManifest:
    - For each name in `enabledTools`, build a JSON object:
      {
        "toolName": {
          "description": "<…>",
          "parameters": { … }
        },
        …
      }
    - Return as JsonObject
*/
JsonObject McpServer::GetToolManifest() const {
    JsonObject manifest;
    for (const String& name : enabledTools) {
        const ToolDefinition* toolDefPtr = allTools.FindPtr(name);
        if (toolDefPtr) {
            JsonObject entry;
            entry("description", toolDefPtr->description);
            entry("parameters", toolDefPtr->parameters); // This assumes parameters is already a JsonObject
            manifest(name) = entry;
        }
    }
    return manifest;
}

/*
  GetPermissions / GetSandboxRoots:
    - Simple getters
*/
Permissions& McpServer::GetPermissions() { return perms; }
const Permissions& McpServer::GetPermissions() const { return perms; }

void McpServer::AddSandboxRoot(const String& root) {
    String normalizedRoot = NormalizePath(root); // U++ Core function
    if (normalizedRoot.IsEmpty()) return;
    if (sandboxRoots.Find(normalizedRoot) < 0) { // Avoid duplicates
        sandboxRoots.Add(normalizedRoot);
        if(logCallback) logCallback("Sandbox root added: " + normalizedRoot);
    }
}
void McpServer::RemoveSandboxRoot(const String& root) {
    String normalizedRoot = NormalizePath(root);
    int index = sandboxRoots.Find(normalizedRoot);
    if (index >= 0) {
        sandboxRoots.Remove(index);
        if(logCallback) logCallback("Sandbox root removed: " + normalizedRoot);
    }
}
const Vector<String>& McpServer::GetSandboxRoots() const {
    return sandboxRoots;
}

/*
  PathUnderRoot:
    - Return true if `child == parent` or `child` starts with `parent + "/"`.
*/
bool McpServer::PathUnderRoot(const String& parent, const String& child) {
    // Normalize paths for robust comparison
    String nParent = NormalizePath(parent);
    String nChild = NormalizePath(child);

    if (nChild == nParent) return true;
    // Ensure parent path ends with a separator for StartsWith check, unless it's a root like "C:/"
    String parentPrefix = nParent;
    if (parentPrefix.GetCount() > 0 && parentPrefix.Last() != DIR_SEPARATOR && parentPrefix.Last() != '\\' && parentPrefix.Last() != '/') {
        parentPrefix.Cat(DIR_SEPARATOR);
    }
    return nChild.StartsWith(parentPrefix);
}

/*
  EnforceSandbox(path):
    - If no sandboxRoots defined, do nothing (but GUI warns earlier).
    - Else, normalize path and check if it’s under any entry in sandboxRoots.
    - If not, `throw String("Sandbox violation: " + normalized)`.
*/
void McpServer::EnforceSandbox(const String& path) const {
    if (sandboxRoots.IsEmpty()) {
        // As per spec, GUI warns, but server allows if no roots defined.
        // if(logCallback) logCallback("Sandbox not enforced: no roots defined. Access to '" + path + "' allowed.");
        return;
    }
    String normalizedPath = NormalizePath(path);
    for (const String& root : sandboxRoots) {
        if (PathUnderRoot(root, normalizedPath)) {
            // if(logCallback) logCallback("Path '" + normalizedPath + "' is within sandbox root '" + root + "'.");
            return; // Path is allowed
        }
    }
    if(logCallback) logCallback("Sandbox violation: Path '" + normalizedPath + "' is not under any defined sandbox root.");
    throw String("Sandbox violation: Access to path '") + normalizedPath + String("' is denied.");
}

/*
  ConfigureBind(bool allInterfaces):
    - Store in `bindAll`.
    - At StartServer(), if bindAll==true, bind to 0.0.0.0; else default (127.0.0.1).
*/
void McpServer::ConfigureBind(bool allInterfaces) {
    bindAll = allInterfaces;
    if(logCallback) logCallback(String("Server configured to bind to ") + (bindAll ? "all interfaces (0.0.0.0)" : "localhost (127.0.0.1)"));
}

/*
  StartServer:
    - If bindAll, instruct WebSocketServer to listen on 0.0.0.0:serverPort,
      else default the base class’s behavior (127.0.0.1:serverPort).
    - Call Listen(); if success, log “Server listening on …” and return true.
    - Otherwise log error and return false.
*/
bool McpServer::StartServer() {
    // U++ WebSocketServer typically uses .Port(int) and .Listen()
    // Binding to all interfaces is often by not specifying an IP or using "0.0.0.0"
    // The Listen method might take an IP. Or there's a BindIP method.
    // WebSocketServer::Listen(port, ip_filter_string, max_pending_connections)
    // Or: Ip(ip_string).Port(port_num).Listen();

    // For now, let's assume the base class has a way to specify bind address before Listen()
    // Or Listen() takes the address. The pseudocode implies the former.
    // The U++ WebSocketServer usually has `Listen(int port = DEFAULT_PORT, const char *host = NULL)`
    // where NULL for host might mean 127.0.0.1 or similar.
    // To bind to all, host is often "0.0.0.0".

    Close(); // Close any existing server first
    Port(serverPort); // Set port

    String bindAddress = bindAll ? "0.0.0.0" : "127.0.0.1";

    // U++ WebSocketServer::Listen(int port, const char* host)
    // The Listen method in U++ might be `bool Listen(int port, const char *host = NULL, int backlog = 5);`
    // Or it might be `bool Listen();` after setting port and IP.
    // The header for UPP WebSockets shows: `bool Listen(int port_ = DEFAULT_PORT, const char *host = NULL, int nlisten = 5);`
    // So, we can use this.

    if (!WebSocketServer::Listen(serverPort, bindAddress)) {
        if(logCallback) logCallback(String("MCP Server failed to start on ") + bindAddress + ":" + AsString(serverPort));
        return false;
    }
    if(logCallback) logCallback(String("MCP Server listening on ") + bindAddress + ":" + AsString(serverPort));
    return true;
}

/*
  StopServer:
    - Call Close() to stop listening/terminate connections.
    - Log “Server stopped.”
*/
void McpServer::StopServer() {
    WebSocketServer::Close(); // Call base class Close
    if (logCallback) logCallback("MCP Server stopped.");
}

/*
  SetPort(int port):
    - Update `serverPort` (to be used at next StartServer call).
*/
void McpServer::SetPort(int newPort) {
    serverPort = newPort;
    if(logCallback) logCallback("Server port set to: " + AsString(serverPort));
}

/*
  SetLogCallback(callback):
    - Store `logCallback = callback;`
*/
void McpServer::SetLogCallback(std::function<void(const String&)> cb) {
    logCallback = std::move(cb);
}

/*
  OnNewConnection(WebSocketSocket& sock):
    1. If client count > maxClientsAllowed, close socket with error code.
       (This is usually handled by WebSocketServer base class via MaxConnections)
    2. Build a manifest message: { "type": "manifest", "tools": { … } } and send.
    3. logCallback("Sent manifest to client").
    4. Bind sock.WhenReceive to a lambda that:
       - Parses JSON: `LoadFromJson(txt)`.
       - If `type=="tool_call"`, call ProcessMessage(sock, parsed).
       - Else send error JSON: { "type":"error", "message": "Unknown message type" }.
    5. Bind sock.WhenClose to a lambda that logs “Client disconnected.”
*/
void McpServer::OnNewConnection(WebSocketSocket& socket) {
    // Client count check is typically handled by MaxConnections setting in U++ WebSocketServer
    // So, if WhenAccept is called, the connection is already accepted past that limit.

    if(logCallback) logCallback(String("New client connected from ") + socket.GetPeerAddr());

    JsonObject manifestMsg;
    manifestMsg("type", "manifest");
    manifestMsg("tools", GetToolManifest());
    SendJson(socket, manifestMsg);

    if(logCallback) logCallback("Sent manifest to client " + socket.GetPeerAddr());

    socket.WhenReceive = [this, &socket](const String& data) {
        if(logCallback) logCallback("Received data from " + socket.GetPeerAddr() + ": " + data);
        Value v = ParseJSON(data); // U++ uses ParseJSON which returns Value
        if (v.IsError()) {
            if(logCallback) logCallback("Failed to parse JSON from client " + socket.GetPeerAddr() + ": " + v.ToString());
            JsonObject errorMsg;
            errorMsg("type", "error");
            errorMsg("message", "Invalid JSON received: " + v.ToString());
            SendJson(socket, errorMsg);
            return;
        }
        if (!v.Is<JsonObject>()) {
             if(logCallback) logCallback("Received JSON is not an object from client " + socket.GetPeerAddr());
             JsonObject errorMsg;
             errorMsg("type", "error");
             errorMsg("message", "JSON payload must be an object.");
             SendJson(socket, errorMsg);
             return;
        }

        JsonObject msgObj = v.To<JsonObject>(); // Convert Value to JsonObject
        String msgType = msgObj.Get("type", Value("")).ToString(); // Get type safely

        if (msgType == "tool_call") {
            ProcessMessage(socket, msgObj);
        } else {
            if(logCallback) logCallback("Unknown message type '" + msgType + "' from client " + socket.GetPeerAddr());
            JsonObject errorMsg;
            errorMsg("type", "error");
            errorMsg("message", String("Unknown message type: ") + msgType);
            SendJson(socket, errorMsg);
        }
    };

    socket.WhenClose = [this, peerAddr = socket.GetPeerAddr()]() { // Capture peerAddr by value
        if(logCallback) logCallback(String("Client disconnected: ") + peerAddr);
    };
}

/*
  ProcessMessage(WebSocketSocket& sock, const JsonObject& msgObj):
    1. Extract `msgType = msgObj("type","")`; if != "tool_call", send error and return. (Already checked by caller)
    2. Extract `toolName = msgObj("tool","")`; if not in allTools or not enabled, send error.
    3. Extract `args = msgObj("args", JsonObject())`; ensure it’s an object.
    4. Retrieve `ToolDefinition def = allTools[toolName]`.
    5. Try:
         JsonValue result = def.func(args);
         SendJson(sock, { "type":"tool_response", "result": result });
         logCallback("Tool '" + toolName + "' executed");
       Catch(const String& e):
         SendJson(sock, { "type":"error", "message": e });
         logCallback("Error in tool '" + toolName + "': " + e);
       Catch(...):
         SendJson(sock, { "type":"error", "message":"Unknown exception" });
         logCallback("Unknown exception in tool '" + toolName + "'");
*/
void McpServer::ProcessMessage(WebSocketSocket& socket, const JsonObject& msgObj) {
    String toolName = msgObj.Get("tool", Value("")).ToString(); // Get tool name safely

    if (toolName.IsEmpty()) {
        if(logCallback) logCallback("Tool call error from " + socket.GetPeerAddr() + ": missing 'tool' field.");
        SendJson(socket, JsonObject("type", "error")("message", "Missing 'tool' field in tool_call."));
        return;
    }

    if (!IsToolEnabled(toolName)) {
        if(logCallback) logCallback("Tool call error from " + socket.GetPeerAddr() + ": tool '" + toolName + "' is not enabled or does not exist.");
        SendJson(socket, JsonObject("type", "error")("message", String("Tool '") + toolName + "' is not available or not enabled."));
        return;
    }

    const ToolDefinition* toolDefPtr = allTools.FindPtr(toolName);
    if (!toolDefPtr || !toolDefPtr->func) { // Should not happen if IsToolEnabled passed and AddTool was robust
        if(logCallback) logCallback("Tool call error from " + socket.GetPeerAddr() + ": tool '" + toolName + "' definition or function is missing internally.");
        SendJson(socket, JsonObject("type", "error")("message", String("Internal server error: Tool '") + toolName + "' is misconfigured."));
        return;
    }

    Value argsValue = msgObj.Get("args", JsonObject()); // Default to empty JsonObject if "args" is missing
    if (!argsValue.Is<JsonObject>()) {
        if(logCallback) logCallback("Tool call error from " + socket.GetPeerAddr() + " for tool '" + toolName + "': 'args' field is not an object.");
        SendJson(socket, JsonObject("type", "error")("message", "'args' field must be a JSON object."));
        return;
    }
    JsonObject args = argsValue.To<JsonObject>();

    try {
        if(logCallback) logCallback("Executing tool '" + toolName + "' for client " + socket.GetPeerAddr() + " with args: " + StoreAsJson(args, true));
        JsonValue result = toolDefPtr->func(args); // JsonValue is an alias for Value in U++

        JsonObject response;
        response("type", "tool_response");
        response("result", result); // result is Value, which JsonObject can take
        SendJson(socket, response);

        if(logCallback) logCallback("Tool '" + toolName + "' executed successfully for " + socket.GetPeerAddr() + ". Result: " + StoreAsJson(result, true));

    } catch (const String& e) {
        if(logCallback) logCallback("Error executing tool '" + toolName + "' for " + socket.GetPeerAddr() + ": " + e);
        SendJson(socket, JsonObject("type", "error")("message", e));
    } catch (const Exc& e) { // U++ general exception
        if(logCallback) logCallback("Exception executing tool '" + toolName + "' for " + socket.GetPeerAddr() + ": " + e);
        SendJson(socket, JsonObject("type", "error")("message", String("Tool execution error: ") + e));
    }
    catch (const std::exception& e) { // Standard C++ exception
        if(logCallback) logCallback("Standard exception executing tool '" + toolName + "' for " + socket.GetPeerAddr() + ": " + e.what());
        SendJson(socket, JsonObject("type", "error")("message", String("Tool execution error: ") + e.what()));
    }
    catch (...) {
        if(logCallback) logCallback("Unknown exception executing tool '" + toolName + "' for " + socket.GetPeerAddr());
        SendJson(socket, JsonObject("type", "error")("message", "An unknown error occurred during tool execution."));
    }
}

/*
  SendJson(WebSocketSocket& sock, const JsonObject& obj):
    - text = StoreAsJson(obj)
    - sock.Send(text)
*/
void McpServer::SendJson(WebSocketSocket& socket, const JsonObject& obj) {
    String text = StoreAsJson(obj, true); // true for pretty print, or false for compact
    socket.SendText(text); // U++ WebSocket uses SendText for string data
    // if(logCallback) logCallback("Sent JSON to " + socket.GetPeerAddr() + ": " + text);
}
