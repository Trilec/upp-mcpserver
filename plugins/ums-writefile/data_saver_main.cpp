#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Core/Json.h> // For JsonObject, JsonArray, Value
#include <Core/ValueUtil.h> // For AsJson for logging (if used)

// Renamed SaveDataToolLogic to WriteFileToolLogic
JsonValue WriteFileToolLogic(McpServer& server, const JsonObject& args){
    if(!server.GetPermissions().allowWriteFiles)throw Exc("Permission denied: Write Files permission is required for 'ums-writefile' tool.");
    String p=args.Get("path","").ToString();
    if(p.IsEmpty())throw Exc("Argument error: 'path' is a required string argument for 'ums-writefile' tool.");
    if(args.Find("data")<0)throw Exc("Argument error: 'data' (string content) is a required argument for 'ums-writefile' tool.");
    String d=args.Get("data","").ToString();
    server.EnforceSandbox(p);
    if(!SaveFile(p,d))throw Exc("File system error: Failed to save data to file '" + p + "' for 'ums-writefile' tool.");
    server.Log("ums-writefile: Data saved successfully to '" + p + "'.");
    return true;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- UMS WriteFile Example ---");
    McpServer server(5005,10);
    server.SetLogCallback([](const String&m){LOG("[S]: "+m);});
    server.GetPermissions().allowWriteFiles=true;
    String sandboxDir=AppendFileName(GetExeFolder(),"ums_example_sandbox_write");
    RealizeDirectory(sandboxDir); // Ensure base sandbox dir exists
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);

    ToolDefinition td;
    td.description="ums-writefile: Writes text to a file at the specified path. Requires Write Files permission and path must be within a sandbox root.";
    td.parameters("path",JsonObject("type","string")("description","Full file path to save the data."))
                 ("data",JsonObject("type","string")("description","Text content to write to the file."));
    td.func=WriteFileToolLogic;

    const String toolName="ums-writefile"; // Updated tool name
    server.AddTool(toolName,td);
    server.EnableTool(toolName);
    LOG("Tool '"+toolName+"' added and enabled.");

    server.ConfigureBind(true);
    if(server.StartServer()){
        LOG("Server started on port 5005. Call tool '"+toolName+"'.");
        String examplePath = AppendFileName(sandboxDir, "output_data_example.txt");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(examplePath) + "\", \"data\": \"This is test data for ums-writefile!\" } }");
        while(true)Sleep(1000);
    } else {
        LOG("Server start failed.");
        SetExitCode(1);
    }
}
