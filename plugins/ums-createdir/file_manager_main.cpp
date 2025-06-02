#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

JsonValue CreateDirToolLogic(McpServer& server, const JsonObject& args){
    if(!server.GetPermissions().allowCreateDirs)throw Exc("Permission denied: Create Directories permission is required for 'ums-createdir' tool.");
    String p=args.Get("path","").ToString();
    if(p.IsEmpty())throw Exc("Argument error: 'path' is a required string argument for 'ums-createdir' tool.");
    server.EnforceSandbox(p);
    if(DirectoryExists(p)) {
        server.Log("ums-createdir: Directory '" + p + "' already exists.");
        return true; // Idempotent
    }
    if(!RealizeDirectory(p))throw Exc("File system error: Failed to create directory '" + p + "' for 'ums-createdir' tool.");
    server.Log("ums-createdir: Directory '" + p + "' created successfully.");
    return true;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- UMS CreateDir Example ---");
    McpServer server(5003,10);
    server.SetLogCallback([](const String&m){LOG("[S]: "+m);});
    server.GetPermissions().allowCreateDirs=true;
    String sandboxDir=AppendFileName(GetExeFolder(),"ums_example_sandbox_create");
    RealizeDirectory(sandboxDir); // Ensure base sandbox dir exists
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);

    ToolDefinition td;
    td.description="ums-createdir: Creates a directory (and any necessary parent directories) at the specified path. Requires Create Directories permission and path must be within a sandbox root.";
    td.parameters("path",JsonObject("type","string")("description","Full path for the new folder."));
    td.func=CreateDirToolLogic; // Direct assignment if signature matches

    const String toolName="ums-createdir"; // Updated tool name
    server.AddTool(toolName,td);
    server.EnableTool(toolName);
    LOG("Tool '"+toolName+"' added and enabled.");

    server.ConfigureBind(true);
    if(server.StartServer()){
        LOG("Server started on port 5003. Call tool '"+toolName+"'.");
        String examplePath = AppendFileName(sandboxDir, "newly_created_dir_example");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(examplePath) + "\" } }");
        while(true)Sleep(1000);
    } else {
        LOG("Server start failed.");
        SetExitCode(1);
    }
}
