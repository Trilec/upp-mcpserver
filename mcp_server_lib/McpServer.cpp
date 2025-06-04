#include "../include/McpServer.h" // Correct path to its own header
#include <Core/Json.h>     // For ParseJSON, StoreAsJson
#include <Core/ValueUtil.h>  // For AsJson, GetErrorText

using namespace Upp;

// Constructor
McpServer::McpServer(int initial_port, const String& initial_path_prefix)
    : serverPort(initial_port),
      ws_path_prefix(initial_path_prefix),
      bindAll(false), // Default to localhost
      is_listening(false) {
    // Ensure path prefix starts with / and does not end with / unless it's just "/"
    if (!this->ws_path_prefix.StartsWith("/")) {
        this->ws_path_prefix = "/" + this->ws_path_prefix;
    }
    if (this->ws_path_prefix.GetCount() > 1 && this->ws_path_prefix.EndsWith("/")) {
        this->ws_path_prefix.TrimLast();
    }
    Log("McpServer object created. Initial port: " + AsString(serverPort) + ", path: " + this->ws_path_prefix);
    // ws_server is default constructed. Callbacks will be set in StartServer.
}

McpServer::~McpServer() {
    Log("McpServer destructor called.");
    if (is_listening) {
        StopServer(); // Ensure server is stopped
    }
    active_clients.Clear();
}

// --- Logging ---
void McpServer::Log(const String& message) {
    if (logCallback) {
        logCallback(message);
    } else {
        RLOG("McpServer: " + message);
    }
}

// --- Tool Management (mostly unchanged from previous versions) ---
void McpServer::AddTool(const String& toolName, const ToolDefinition& toolDef) {
    allTools.GetAdd(toolName) = toolDef;
    Log("Tool added: " + toolName);
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
    Log("GetToolManifest() constructing 'tools' object for manifest.");
    JsonObject tools_payload;
    for (const String& tool_name : enabledTools) {
        const ToolDefinition* def = allTools.FindPtr(tool_name);
        if (def) {
            JsonObject tool_detail;
            tool_detail("description", def->description);
            tool_detail("parameters", def->parameters);
            tools_payload(tool_name, tool_detail);
        } else {
            Log("Warning: Enabled tool '" + tool_name + "' not found in allTools map during manifest creation.");
        }
    }
    return tools_payload;
}

// --- Permissions & Sandbox (unchanged) ---
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
    if(sandboxRoots.RemoveKey(NormalizePath(root)) > 0) {
         Log("Sandbox root removed: " + NormalizePath(root));
    }
}
void McpServer::EnforceSandbox(const String& path) const {
    if (sandboxRoots.IsEmpty()) {
        Log("Warning: EnforceSandbox called but no sandbox roots defined. Path '" + path + "' allowed by default.");
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

// --- Server Configuration & Control (using Upp::Ws) ---
void McpServer::ConfigureBind(bool all) {
    if(is_listening){Log("ConfigureBind: Cannot change bind address while server is running."); return;}
    bindAll = all; Log("Bind all interfaces set to: " + AsString(bindAll));
}
void McpServer::SetPort(uint16 port) {
    if(is_listening){Log("SetPort: Cannot change port while server is running."); return;}
    if(port==0){Log("SetPort: Invalid port 0. Port not changed."); return;}
    serverPort = port; Log("Server port set to: " + AsString(serverPort));
}
void McpServer::SetPathPrefix(const String& path) {
    if(is_listening){Log("SetPathPrefix: Cannot change path prefix while server is running."); return;}
    ws_path_prefix = path.StartsWith("/") ? path : "/" + path;
    if(ws_path_prefix.GetCount() > 1 && ws_path_prefix.EndsWith("/")) ws_path_prefix.TrimLast();
    Log("WebSocket path prefix set to: " + ws_path_prefix);
}
void McpServer::SetTls(bool use_tls_flag, const String& cert_path, const String& key_path) {
    if(is_listening){Log("SetTls: Cannot change TLS settings while server is running."); return;}
    use_tls = use_tls_flag;
    tls_cert_path = cert_path;
    tls_key_path = key_path;
    Log("TLS settings updated. Use TLS: " + AsString(use_tls));
}

bool McpServer::StartServer() {
    if (is_listening) {
        Log("StartServer: Server is already running on port " + AsString(serverPort) + ".");
        return true;
    }
    Log("Attempting to start Upp::Ws::Server on port " + AsString(serverPort) + " path " + ws_path_prefix + " TLS: " + AsString(use_tls));
    ws_server.WhenAccept = THISBACK(OnWsAccept);

    if (!ws_server.Listen(serverPort, ws_path_prefix, use_tls, tls_cert_path, tls_key_path)) {
        Log("StartServer FAILED: Upp::Ws::Server::Listen() failed. System Error: " + GetLastSystemError());
        is_listening = false;
        return false;
    }
    is_listening = true;
    Log("StartServer SUCCEEDED: Upp::Ws::Server is now listening.");
    return true;
}

bool McpServer::StopServer() {
    if (!is_listening) {
        Log("StopServer: Server is not currently running.");
        return true;
    }
    Log("Attempting to stop Upp::Ws::Server...");

    for(int i = 0; i < active_clients.GetCount(); i++) {
        Upp::Ws::Endpoint* ep = active_clients.GetKey(i);
        if(ep && !ep->IsClosed()) {
            Log("StopServer: Closing active client endpoint: " + ep->GetSocket().GetPeerAddr());
            ep->Close(1001, "Server shutting down");
        }
    }
    active_clients.Clear();

    // Assuming Ws::Server needs explicit listener close or similar
    // If Ws::Server's destructor handles closing the listener, this might not be strictly necessary
    // but it's good practice to have an explicit stop for symmetry with Listen.
    // The Ws::Server sketch doesn't show an explicit StopListening method.
    // We rely on PumpEvents not being called and eventually the Ws::Server destructor.
    // For now, setting is_listening to false is key.
    // If Ws::Server listener needs explicit close: ws_server.listener.Close(); (if accessible)

    is_listening = false;
    Log("StopServer: Server has been requested to stop. Active clients closed. PumpEvents should no longer be called.");
    return true;
}

void McpServer::PumpEvents() {
    if (is_listening) {
        ws_server.Pump();
        // Also pump individual clients if Ws::Endpoint::Pump() is needed and not handled by Ws::Server::Pump()
        // for(int i = 0; i < active_clients.GetCount(); ++i) {
        //     Upp::Ws::Endpoint* ep = active_clients.GetKey(i);
        //     if(ep && !ep->Pump()) { // If Pump returns false, indicates error
        //         OnWsClientError(ep, -1); // Simulate an error code
        //     }
        // }
    }
}

void McpServer::SetLogCallback(std::function<void(const String&)> cb) { logCallback = cb; }

// --- Upp::Ws Callbacks Implementation ---
void McpServer::OnWsAccept(Upp::Ws::Endpoint& client_endpoint) {
    String client_ip = client_endpoint.GetSocket().GetPeerAddr();
    Log("OnWsAccept: New WebSocket connection from " + client_ip);

    active_clients.Add(&client_endpoint);

    client_endpoint.WhenText = THISBACK2(OnWsText, &client_endpoint);
    client_endpoint.WhenBinary = THISBACK2(OnWsBinary, &client_endpoint);
    client_endpoint.WhenClose = Gate<int, const String&>(THISBACK3(OnWsClientClose, &client_endpoint)); // Correct binding for Gate2
    client_endpoint.WhenError = THISBACK2(OnWsClientError, &client_endpoint);

    JsonObject manifest_message;
    manifest_message("type", "manifest");
    manifest_message("tools", GetToolManifest());
    SendJsonResponse(&client_endpoint, manifest_message);
    Log("Manifest sent to " + client_ip);
}

void McpServer::OnWsText(Upp::Ws::Endpoint* client_endpoint, String msg) { // msg is String, not const String&
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Log("OnWsText from " + client_ip + ": " + msg);
    ProcessMcpMessage(client_endpoint, msg); // Pass raw string to ProcessMcpMessage
}

void McpServer::OnWsBinary(Upp::Ws::Endpoint* client_endpoint, String data) { // data is String
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Log("OnWsBinary from " + client_ip + ": received " + AsString(data.GetCount()) + " bytes. (Data not processed)");
}

bool McpServer::OnWsClientClose(Upp::Ws::Endpoint* client_endpoint, int code, const String& reason) {
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Log("OnWsClientClose: Client " + client_ip + " disconnected. Code: " + AsString(code) + ", Reason: '" + reason + "'");
    active_clients.RemoveKey(client_endpoint);
    return true;
}

void McpServer::OnWsClientError(Upp::Ws::Endpoint* client_endpoint, int error_code) {
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Log("OnWsClientError: Error for client " + client_ip + ". Code: " + AsString(error_code) +
        ". System: " + GetLastSystemError());
    active_clients.RemoveKey(client_endpoint);
}

void McpServer::ProcessMcpMessage(Upp::Ws::Endpoint* client_endpoint, const String& message_text) {
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Value parsed_json = ParseJSON(message_text);
    if (parsed_json.IsError()) {
        Log("JSON parse error from " + client_ip + ": " + GetErrorText(parsed_json));
        SendJsonResponse(client_endpoint, JsonObject("type","error")("message","Invalid JSON: "+GetErrorText(parsed_json)));
        return;
    }
    if (!parsed_json.Is<JsonObject>()) {
        Log("Invalid msg from " + client_ip + ": not a JSON object.");
        SendJsonResponse(client_endpoint, JsonObject("type","error")("message","Payload must be JSON object."));
        return;
    }

    JsonObject msgObj = parsed_json.To<JsonObject>();
    String msgType = msgObj.Get("type", "").ToString();

    if (msgType == "tool_call") {
        String toolName = msgObj.Get("tool", "").ToString();
        if (toolName.IsEmpty()) {
            Log("Tool call error from " + client_ip + ": 'tool' field missing.");
            SendJsonResponse(client_endpoint, JsonObject("type", "error")("message", "Missing 'tool' field."));
            return;
        }
        Value argsValue = msgObj.Get("args", JsonObject());
        if (!argsValue.Is<JsonObject>()) {
            Log("Tool call error from " + client_ip + " for '" + toolName + "': 'args' not an object.");
            SendJsonResponse(client_endpoint, JsonObject("type", "error")("message", "'args' must be an object."));
            return;
        }
        JsonObject args = argsValue.To<JsonObject>();
        Log("Client " + client_ip + " requests tool '" + toolName + "' with args: " + AsJson(args));

        const ToolDefinition* toolDefPtr = allTools.FindPtr(toolName);
        if (!toolDefPtr) { /* ... error handling ... */ Log("Tool '" + toolName + "' not found."); SendJsonResponse(client_endpoint, JsonObject("type","error")("message", "Tool not found: " + toolName)); return; }
        if (!IsToolEnabled(toolName)) { /* ... error handling ... */ Log("Tool '" + toolName + "' not enabled."); SendJsonResponse(client_endpoint, JsonObject("type","error")("message", "Tool not enabled: " + toolName)); return; }
        if (!toolDefPtr->func) { /* ... error handling ... */ Log("Tool '" + toolName + "' no function."); SendJsonResponse(client_endpoint, JsonObject("type","error")("message", "Server error: Tool misconfigured " + toolName)); return; }

        try {
            Log("Executing tool '" + toolName + "' for " + client_ip);
            Value result = toolDefPtr->func(args);
            SendJsonResponse(client_endpoint, JsonObject("type", "tool_response")("result", result));
            Log("Tool '" + toolName + "' success for " + client_ip + ". Result: " + AsJson(result, true));
        } catch (const Exc& e) { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message", e)); }
          catch (const String& e_str) { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message", e_str)); }
          catch (const std::exception& e_std) { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message", String("StdExc: ") + e_std.what())); }
          catch (...) { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message", "Unknown error in tool " + toolName)); }
    } else if (msgType.IsEmpty()) { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message","'type' field missing.")); }
      else { /* ... */ SendJsonResponse(client_endpoint, JsonObject("type","error")("message","Unknown message type: "+msgType));}
}

void McpServer::SendJsonResponse(Upp::Ws::Endpoint* client, const JsonObject& obj) {
    if (!client || client->IsClosed()) {
        Log("SendJsonResponse: Client is null or already closed. Cannot send.");
        if(client) active_clients.RemoveKey(client);
        return;
    }
    String json_text = StoreAsJson(obj, false);
    // Log("Sending to client " + client->GetSocket().GetPeerAddr() + ": " + json_text); // Verbose
    if (!client->SendText(json_text)) { // Assuming Ws::Endpoint::SendText returns bool success
        Log("Error: Failed to send JSON to client " + client->GetSocket().GetPeerAddr() + ". System error: " + GetLastSystemError());
        active_clients.RemoveKey(client);
    }
}
