#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Core/Json.h>
#include <Core/ValueUtil.h>

using namespace Upp;

// Tool logic now takes const Value& args
Value ReadFileToolLogic_Plugin(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>()) throw Exc("ums-readfile-plugin: 'args' must be a JSON object.");
    ValueMap args = args_v.Get<ValueMap>();
    server.Log("ums-readfile-plugin invoked. Args: " + StoreAsJson(args_v, true));
    if (!server.GetPermissions().allowReadFiles) throw Exc("Permission denied: Read Files required.");
    String path = args.Get("path", "").ToString(); if (path.IsEmpty()) throw Exc("Argument error: 'path' required.");
    server.EnforceSandbox(path); String content = LoadFile(path);
    if (content.IsVoid()) throw Exc("File error: Could not read file '" + path + "'.");
    return content;
}
CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0); LOG("--- UMS File Reader Plugin ---");
    McpServer server(5001, 10); server.SetLogCallback([](const String&m){LOG("[Svc]: "+m);});
    server.GetPermissions().allowReadFiles = true;
    String sandboxDir=AppendFileName(GetExeFolder(),"ums_plugin_sandbox_rf");RealizeDirectory(sandboxDir);
    server.AddSandboxRoot(sandboxDir);
    String testFilePath = AppendFileName(sandboxDir,"test.txt");
    SaveFile(testFilePath,"Hello from ums-readfile plugin!");
    LOG("Test file created at: " + testFilePath);

    ToolDefinition td; td.description = "ums-readfile-plugin: Reads file. Needs Read Files & sandbox.";
    ValueMap params_map; params_map.Add("path",ValueMap("type","string")("description","File path"));
    td.parameters = Value(params_map);

    td.func = [&](const Value&args_v){return ReadFileToolLogic_Plugin(server,args_v);};
    const String toolName = "ums-readfile-plugin";
    server.AddTool(toolName, td); server.EnableTool(toolName); LOG("Tool '"+toolName+"' enabled.");
    server.ConfigureBind(true); if(server.StartServer()){
        LOG("Server on port 5001. Call tool '"+toolName+"'.");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(testFilePath) + "\" } }");
        while(true)Sleep(1000);
    }
    else{LOG("Server start failed.");SetExitCode(1);}
}
