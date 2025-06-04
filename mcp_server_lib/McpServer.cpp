#include "../include/McpServer.h"
#include <Core/Json.h>     // For ParseJSON, StoreAsJson
#include <Core/ValueUtil.h>  // For AsJson, GetErrorText (Value::ToString for errors)

using namespace Upp;

McpServer::McpServer(int initial_port, const String& initial_path_prefix)
  : serverPort(initial_port), ws_path_prefix(initial_path_prefix), bindAll(false), use_tls(false), is_listening(false) {
    if (!this->ws_path_prefix.StartsWith("/")) { this->ws_path_prefix = "/" + this->ws_path_prefix; }
    if (this->ws_path_prefix.GetCount() > 1 && this->ws_path_prefix.EndsWith("/")) { this->ws_path_prefix.TrimLast(); }
    Log("McpServer object created. Initial port: " + AsString(serverPort) + ", path: " + this->ws_path_prefix);
}
McpServer::~McpServer() {
    Log("McpServer destructor called."); if (is_listening) StopServer(); active_clients.Clear();
}
void McpServer::Log(const String& message) { if (logCallback) logCallback(message); else RLOG("McpServer: " + message); }
void McpServer::AddTool(const String& toolName, const ToolDefinition& toolDef) { allTools.GetAdd(toolName) = toolDef; Log("Tool added: " + toolName); }
Vector<String> McpServer::GetAllToolNames() const { return allTools.GetKeys(); }
void McpServer::EnableTool(const String& toolName) { if (allTools.FindPtr(toolName)) { enabledTools.Add(toolName); Log("Tool enabled: " + toolName); } else { Log("Warning: Attempt to enable non-existent tool: " + toolName); }}
void McpServer::DisableTool(const String& toolName) { enabledTools.RemoveKey(toolName); Log("Tool disabled: " + toolName); }
bool McpServer::IsToolEnabled(const String& toolName) const { return enabledTools.Find(toolName) >= 0; }

Value McpServer::GetToolManifest() const {
    Log("GetToolManifest() constructing 'tools' ValueMap.");
    ValueMap tools_payload_map;
    for(const String& tool_name : enabledTools) {
        const ToolDefinition* def = allTools.FindPtr(tool_name);
        if(def) {
            ValueMap tool_detail_map;
            tool_detail_map.Add("description", def->description);
            tool_detail_map.Add("parameters", def->parameters);
            tools_payload_map.Add(tool_name, Value(tool_detail_map));
        } else { Log("Warning: Enabled tool '" + tool_name + "' not found. Skipping from manifest."); }
    }
    return Value(tools_payload_map);
}

Permissions& McpServer::GetPermissions() { return perms; }
const Permissions& McpServer::GetPermissions() const { return perms; }
Vector<String>& McpServer::GetSandboxRoots() { return sandboxRoots; }
const Vector<String>& McpServer::GetSandboxRoots() const { return sandboxRoots; }
void McpServer::AddSandboxRoot(const String& root) { String nr=NormalizePath(root); if(nr.IsEmpty())return; if(sandboxRoots.Find(nr)<0)sandboxRoots.Add(nr); Log("Sandbox root added: "+nr); }
void McpServer::RemoveSandboxRoot(const String& root) { if(sandboxRoots.RemoveKey(NormalizePath(root)) > 0) Log("Sandbox root removed: "+NormalizePath(root));}
void McpServer::EnforceSandbox(const String& path) const { if(sandboxRoots.IsEmpty()){Log("Warn: EnforceSandbox no roots for '"+path+"'.");return;} String np=NormalizePath(path); for(const String&r:sandboxRoots){if(PathUnderRoot(r,np))return;} throw Exc("Sandbox violation: Path '"+np+"' outside roots.");}
bool McpServer::PathUnderRoot(const String&p,const String&c){String np=NormalizePath(p),nc=NormalizePath(c); if(nc==np)return true; String pp=np; if(pp.GetCount()>0&&pp.Last()!=DIR_SEPARATOR&&pp.Last()!='\\' && pp.Last()!='/')pp.Cat(DIR_SEPARATOR); return nc.StartsWith(pp);}

void McpServer::ConfigureBind(bool all){if(is_listening){Log("Err: Bind change while running.");return;}bindAll=all;Log("BindAll: "+AsString(all));}
void McpServer::SetPort(uint16 port){if(is_listening){Log("Err: Port change while running.");return;}if(port==0){Log("Err: Invalid port 0.");return;}serverPort=port;Log("Port set: "+AsString(port));}
void McpServer::SetPathPrefix(const String&path){if(is_listening){Log("Err: Path change while running.");return;}ws_path_prefix=path.StartsWith("/")?path:"/"+path;if(ws_path_prefix.GetCount()>1&&ws_path_prefix.EndsWith("/"))ws_path_prefix.TrimLast();Log("PathPrefix: "+ws_path_prefix);}
void McpServer::SetTls(bool ut,const String&cp,const String&kp){if(is_listening){Log("Err: TLS change while running.");return;}use_tls=ut;tls_cert_path=cp;tls_key_path=kp;Log("TLS use: "+AsString(ut));}

bool McpServer::StartServer(){if(is_listening){Log("Already running.");return true;}Log("Starting Ws::Server...");ws_server.WhenAccept=THISBACK(OnWsAccept);if(!ws_server.Listen(serverPort,ws_path_prefix,use_tls,tls_cert_path,tls_key_path)){Log("StartServer FAILED: Listen failed. SysErr: "+GetLastSystemError());is_listening=false;return false;}is_listening=true;Log("StartServer SUCCEEDED. Listening on "+AsString(serverPort)+ws_path_prefix);return true;}
bool McpServer::StopServer(){if(!is_listening){Log("Not running.");return true;}Log("Stopping Ws::Server...");for(int i=0;i<active_clients.GetCount();i++){Upp::Ws::Endpoint*ep=active_clients.GetKey(i);if(ep&&!ep->IsClosed()){Log("Closing client: "+ep->GetSocket().GetPeerAddr());ep->Close(1001,"Server shutdown");}}active_clients.Clear();is_listening=false;Log("Server stopped. Pump should cease.");return true;}
void McpServer::PumpEvents(){if(is_listening)ws_server.Pump();}
void McpServer::SetLogCallback(std::function<void(const String&)> cb){logCallback=cb;}

void McpServer::OnWsAccept(Upp::Ws::Endpoint& client_endpoint) {
    String client_ip = client_endpoint.GetSocket().GetPeerAddr(); Log("OnWsAccept: New conn from " + client_ip);
    active_clients.Add(&client_endpoint);
    client_endpoint.WhenText = THISBACK2(OnWsText, &client_endpoint);
    client_endpoint.WhenBinary = THISBACK2(OnWsBinary, &client_endpoint);
    client_endpoint.WhenClose = Gate<int, const String&>(THISBACK3(OnWsClientClose, &client_endpoint));
    client_endpoint.WhenError = THISBACK2(OnWsClientError, &client_endpoint);
    ValueMap manifest_msg_map;
    manifest_msg_map.Add("type", "manifest");
    manifest_msg_map.Add("tools", GetToolManifest());
    SendJsonResponse(&client_endpoint, Value(manifest_msg_map)); Log("Manifest sent to " + client_ip);
}

void McpServer::OnWsText(Upp::Ws::Endpoint* client_endpoint, String msg) {
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Log("OnWsText from " + client_ip + ": " + msg);
    ProcessMcpMessage(client_endpoint, msg);
}

void McpServer::ProcessMcpMessage(Upp::Ws::Endpoint* client_endpoint, const String& message_text) {
    String client_ip = client_endpoint->GetSocket().GetPeerAddr();
    Value parsed_json = ParseJSON(message_text);
    if(parsed_json.IsError()){Log("JSON parse err from "+client_ip+": "+GetErrorText(parsed_json));SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Invalid JSON: "+GetErrorText(parsed_json))));return;}
    if(!parsed_json.Is<ValueMap>()){Log("Invalid msg from "+client_ip+": not JSON object.");SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Payload must be JSON object.")));return;}

    ValueMap msg_map = parsed_json.Get<ValueMap>();
    String msgType = msg_map.Get("type", Value("")).ToString(); // Use Value("") as default for Get

    if(msgType == "tool_call"){
        String toolName = msg_map.Get("tool", Value("")).ToString();
        if(toolName.IsEmpty()){Log("Tool call err from "+client_ip+": 'tool' missing.");SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","'tool' field missing.")));return;}

        Value args_value = msg_map.Get("args", Value(ValueMap()));
        // ToolFunc expects const Value& args, where args is expected to be a ValueMap by the tool logic.
        // No need to check Is<ValueMap>() here if tools do it, but good for robustness.
        if(!args_value.Is<ValueMap>()) {
             Log("Tool call err from "+client_ip+" for '"+toolName+"': 'args' not ValueMap object.");
             SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","'args' must be a JSON object.")));
             return;
        }

        Log("Client "+client_ip+" tool '"+toolName+"' args: "+StoreAsJson(args_value, true));
        const ToolDefinition* toolDefPtr = allTools.FindPtr(toolName);
        if(!toolDefPtr){Log("Tool '"+toolName+"' not found. Req from "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Tool '"+toolName+"' not found.")));return;}
        if(!IsToolEnabled(toolName)){Log("Tool '"+toolName+"' not enabled. Req from "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Tool '"+toolName+"' not enabled.")));return;}
        if(!toolDefPtr->func){Log("CRITICAL: Tool '"+toolName+"' no func! Req from "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Server Error: Tool '"+toolName+"' misconfigured.")));return;}
        try{
            Log("Executing tool '"+toolName+"' for "+client_ip);
            Value result = toolDefPtr->func(args_value);
            SendJsonResponse(client_endpoint, Value(ValueMap("type","tool_response")("result",result)));
            Log("Tool '"+toolName+"' success for "+client_ip+". Result: "+StoreAsJson(result,true));
        }catch(const Exc&e){Log("Tool '"+toolName+"' err(Exc) for "+client_ip+": "+e.ToString());SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message",e.ToString())));}
        catch(const String&e_str){Log("Tool '"+toolName+"' err(String) for "+client_ip+": "+e_str);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message",e_str)));}
        catch(const std::exception&e_std){Log("Tool '"+toolName+"' err(std::exc) for "+client_ip+": "+e_std.what());SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message",String("StdExc: ")+e_std.what())));}
        catch(...){Log("Tool '"+toolName+"' err(unknown) for "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Unknown error in tool '"+toolName+"'.")));}
    } else if(msgType.IsEmpty()){Log("Msg type missing from "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","'type' field missing.")));}
    else {Log("Unknown msg type '"+msgType+"' from "+client_ip);SendJsonResponse(client_endpoint,Value(ValueMap("type","error")("message","Unknown type: "+msgType)));}
}

void McpServer::OnWsBinary(Upp::Ws::Endpoint*ep,String d){String cip=active_clients.Get(ep,"UnkIP");Log("Binary from "+cip+": "+AsString(d.GetCount())+"B.");}
bool McpServer::OnWsClientClose(Upp::Ws::Endpoint*ep,int c,const String&r){String cip=active_clients.Get(ep,"UnkIP");Log("Client "+cip+" closed. Code:"+AsString(c)+", Reason:'"+r+"'");active_clients.RemoveKey(ep);return true;}
void McpServer::OnWsClientError(Upp::Ws::Endpoint*ep,int ec){String cip=active_clients.Get(ep,"UnkIP");Log("Client err "+cip+". Code:"+AsString(ec)+". Sys:"+GetLastSystemError());active_clients.RemoveKey(ep);}

void McpServer::SendJsonResponse(Upp::Ws::Endpoint* client, const Value& jsonData) {
    if(!client||client->IsClosed()){Log("SendJsonResponse: Client null/closed.");if(client)active_clients.RemoveKey(client);return;}
    String json_text = StoreAsJson(jsonData, false);
    // Log("Sending to client "+client->GetSocket().GetPeerAddr()+": "+json_text); // Verbose
    if(!client->SendText(json_text)){Log("ERR: SendText failed to client "+client->GetSocket().GetPeerAddr()+". SysErr: "+GetLastSystemError());active_clients.RemoveKey(client);}
}
