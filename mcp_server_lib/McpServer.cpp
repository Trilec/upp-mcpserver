#include "McpServer.h"
// #include <Json/JsonValue.h> // Value is in Core/Value.h, JsonObject in Core/Json.h (via WebSockets.h)
#include <Core/ValueUtil.h> // For AsJson, GetErrorText

McpServer::McpServer(int port, int maxClients)
  : WebSocketServer(),
    maxClientsAllowed(maxClients), bindAll(false), serverPort(port), is_listening(false) {
    Port(port); MaxConnections(maxClientsAllowed);
    WhenAccept = THISBACK(OnNewConnection);
    // Log("McpServer initialized. Port: " + AsString(port)); // Logged by Main.cpp
}

void McpServer::Log(const String& message) {
    if (logCallback) logCallback(message);
}

void McpServer::AddTool(const String& toolName, const ToolDefinition& toolDef) {
    allTools.GetAdd(toolName) = toolDef; Log("Tool added: " + toolName);
}
Vector<String> McpServer::GetAllToolNames() const { return allTools.GetKeys(); }

void McpServer::EnableTool(const String& toolName) {
    if (allTools.FindPtr(toolName)) {
        enabledTools.Add(toolName); Log("Tool enabled: " + toolName);
    } else { Log("Warning: Attempt to enable non-existent tool: " + toolName); }
}
void McpServer::DisableTool(const String& toolName) {
    enabledTools.RemoveKey(toolName); Log("Tool disabled: " + toolName);
}
bool McpServer::IsToolEnabled(const String& toolName) const { return enabledTools.Find(toolName) >= 0; }

JsonObject McpServer::GetToolManifest() const {
    Log("GetToolManifest() called."); JsonObject manifest_payload;
    for(const String& tool_name : enabledTools) {
        const ToolDefinition* def = allTools.FindPtr(tool_name);
        if(def) {
            JsonObject tool_detail;
            tool_detail("description", def->description);
            tool_detail("parameters", def->parameters);
            manifest_payload(tool_name, tool_detail);
        }
    }
    return manifest_payload;
}

Permissions& McpServer::GetPermissions() { return perms; }
const Permissions& McpServer::GetPermissions() const { return perms; }
Vector<String>& McpServer::GetSandboxRoots() { return sandboxRoots; }
const Vector<String>& McpServer::GetSandboxRoots() const { return sandboxRoots; }
void McpServer::AddSandboxRoot(const String& root) {
    String normRoot = NormalizePath(root); if(normRoot.IsEmpty()) return;
    if(sandboxRoots.Find(normRoot) < 0) { // Avoid duplicates
        sandboxRoots.Add(normRoot);
        Log("Sandbox root added: " + normRoot);
    }
}
void McpServer::RemoveSandboxRoot(const String& root) {
    if(sandboxRoots.RemoveKey(NormalizePath(root)) > 0) { // Check if any removed
        Log("Sandbox root removed: " + NormalizePath(root));
    }
}
void McpServer::EnforceSandbox(const String& path) const {
    if (sandboxRoots.IsEmpty()) {
        Log("Warning: EnforceSandbox called but no sandbox roots are defined. Path '" + path + "' allowed by default in this scenario.");
        return;
    }
    String normalizedPath = NormalizePath(path);
    for (const String& r : sandboxRoots) { if (PathUnderRoot(r, normalizedPath)) return; }
    throw Exc("Sandbox violation: Path '" + normalizedPath + "' is outside defined sandbox roots.");
}
bool McpServer::PathUnderRoot(const String& parent, const String& child) {
    String nParent = NormalizePath(parent); String nChild = NormalizePath(child);
    if (nChild == nParent) return true;
    String parentPrefix = nParent;
    if (parentPrefix.GetCount() > 0 && parentPrefix.Last()!=DIR_SEPARATOR && parentPrefix.Last()!='\\' && parentPrefix.Last()!='/') {
        parentPrefix.Cat(DIR_SEPARATOR);
    }
    return nChild.StartsWith(parentPrefix);
}

void McpServer::ConfigureBind(bool all) {
    if(is_listening){Log("ConfigureBind: Cannot change while server is running."); return;}
    bindAll = all; Log("Bind all interfaces set to: " + AsString(bindAll));
}
void McpServer::SetPort(int port) {
    if(is_listening){Log("SetPort: Cannot change while server is running."); return;}
    if(port<=0||port>65535){Log("SetPort: Invalid port: "+AsString(port)); return;}
    serverPort = port; Port(serverPort);
    Log("Server port set to: " + AsString(serverPort));
}
String McpServer::GetListenHost() const { return bindAll ? "0.0.0.0" : "127.0.0.1"; }

bool McpServer::StartServer() {
    if(is_listening){Log("StartServer: Already running on port "+AsString(serverPort)+"."); return true;}
    Log("Starting server on "+GetListenHost()+":"+AsString(serverPort)+"...");
    String hostToBind = bindAll ? "0.0.0.0" : nullptr;
    if(!WebSocketServer::Listen(serverPort, hostToBind)){
        Log("StartServer FAILED: Unable to listen. Error: "+GetLastErrorMessage());
        is_listening = false; return false;
    }
    is_listening = true;
    Log("StartServer SUCCEEDED: Listening on "+ (WebSocketServer::GetHost().IsEmpty() ? GetListenHost() : WebSocketServer::GetHost()) +":"+AsString(WebSocketServer::GetPort())+".");
    return true;
}
bool McpServer::StopServer() {
    if(!is_listening){Log("StopServer: Not running."); return true;}
    Log("Stopping server..."); WebSocketServer::Close(); is_listening = false;
    Log("StopServer SUCCEEDED: Server stopped."); return true;
}
void McpServer::SetLogCallback(std::function<void(const String&)> cb) { logCallback = cb; }

void McpServer::OnNewConnection(WebSocket& socket) {
    String clientAddr = socket.GetPeerAddr();
    Log("New client from IP: " + clientAddr);
    JsonObject manifestMsg; manifestMsg("type", "manifest")("tools", GetToolManifest());
    SendJson(socket, manifestMsg); Log("Sent manifest to client: " + clientAddr);

    socket.WhenReceive = [this, &socket, clientAddr_captured = clientAddr]() {
        String data = socket.Receive();
        Log("Rx from " + clientAddr_captured + ": " + data);
        Value parsed_json = ParseJSON(data);
        if(parsed_json.IsError()){
            Log("JSON parse error from " + clientAddr_captured + ": " + GetErrorText(parsed_json));
            SendJson(socket, JsonObject("type","error")("message","Invalid JSON: "+GetErrorText(parsed_json))); return;
        }
        if(!parsed_json.Is<JsonObject>()){
            Log("Invalid msg from " + clientAddr_captured + ": not a JSON object.");
            SendJson(socket, JsonObject("type","error")("message","Payload must be JSON object.")); return;
        }
        JsonObject msgObj = parsed_json.To<JsonObject>();
        String msgType = msgObj.Get("type", Value("")).ToString();
        if(msgType == "tool_call"){ ProcessMessage(socket, msgObj); }
        else if(msgType.IsEmpty()){ Log("Msg type missing from "+clientAddr_captured); SendJson(socket, JsonObject("type","error")("message","'type' field missing."));}
        else{ Log("Unknown msg type '"+msgType+"' from "+clientAddr_captured); SendJson(socket, JsonObject("type","error")("message","Unknown type: "+msgType));}
    };
    socket.WhenClose = [this, clientAddr_captured = clientAddr]() { Log("Client disconnected: " + clientAddr_captured); };
}

// --- Functional ProcessMessage ---
void McpServer::ProcessMessage(WebSocket& socket, const JsonObject& msgObj) {
    String clientAddr = socket.GetPeerAddr();
    // Log("ProcessMessage invoked for client: " + clientAddr + ". Full Message: " + AsJson(msgObj)); // Can be verbose

    String toolName = msgObj.Get("tool", "").ToString();
    if (toolName.IsEmpty()) {
        Log("Tool call error from " + clientAddr + ": 'tool' field missing or empty.");
        SendJson(socket, JsonObject("type", "error")("message", "Missing 'tool' field in tool_call."));
        return;
    }

    Value argsValue = msgObj.Get("args", JsonObject());
    if (!argsValue.Is<JsonObject>()) {
        Log("Tool call error from " + clientAddr + " for tool '" + toolName + "': 'args' field is not an object.");
        SendJson(socket, JsonObject("type", "error")("message", "'args' field must be a JSON object for tool '" + toolName + "'."));
        return;
    }
    JsonObject args = argsValue.To<JsonObject>();

    Log("Client " + clientAddr + " requests tool '" + toolName + "' with args: " + AsJson(args));

    const ToolDefinition* toolDefPtr = allTools.FindPtr(toolName);
    if (!toolDefPtr) {
        Log("Tool '" + toolName + "' not found (not registered). Request from " + clientAddr);
        SendJson(socket, JsonObject("type", "error")("message", "Tool '" + toolName + "' not found."));
        return;
    }
    if (!IsToolEnabled(toolName)) {
        Log("Tool '" + toolName + "' is not enabled. Request from " + clientAddr);
        SendJson(socket, JsonObject("type", "error")("message", "Tool '" + toolName + "' is not currently enabled."));
        return;
    }
    if (!toolDefPtr->func) {
        Log("CRITICAL: Tool '" + toolName + "' has no function defined! Request from " + clientAddr);
        SendJson(socket, JsonObject("type", "error")("message", "Internal Server Error: Tool '" + toolName + "' is misconfigured (no function)."));
        return;
    }

    try {
        Log("Executing tool '" + toolName + "' for client " + clientAddr + "...");
        Value result = toolDefPtr->func(args);

        JsonObject response;
        response("type", "tool_response");
        response("result", result);
        SendJson(socket, response);

        Log("Tool '" + toolName + "' executed successfully for " + clientAddr + ". Result: " + (result.IsError() ? result.ToString() : AsJson(result,true)) ); // Log error string directly

    } catch (const Exc& e) {
        Log("Tool '" + toolName + "' execution error (Exc) for " + clientAddr + ": " + e);
        SendJson(socket, JsonObject("type", "error")("message", e));
    } catch (const String& e_str) {
        Log("Tool '" + toolName + "' execution error (String) for " + clientAddr + ": " + e_str);
        SendJson(socket, JsonObject("type", "error")("message", e_str));
    } catch (const std::exception& e_std) {
        Log("Tool '" + toolName + "' execution error (std::exception) for " + clientAddr + ": " + e_std.what());
        SendJson(socket, JsonObject("type", "error")("message", String("Standard Exception: ") + e_std.what()));
    } catch (...) {
        Log("Tool '" + toolName + "' execution error (unknown type) for " + clientAddr + ".");
        SendJson(socket, JsonObject("type", "error")("message", "An unknown error occurred during tool '" + toolName + "' execution."));
    }
}

// SendJson will be fully implemented in Step 5.
void McpServer::SendJson(WebSocket& socket, const JsonObject& obj) {
    String clientAddr = socket.GetPeerAddr(); // May be empty if socket closed prematurely
    String json_text = StoreAsJson(obj, false); // Send compact JSON
    // Log("Sending JSON to " + clientAddr + ": " + json_text); // Can be verbose, log specific sends if needed
    socket.SendText(json_text);
}
