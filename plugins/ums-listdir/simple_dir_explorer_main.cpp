#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

JsonValue ListDirToolLogic(McpServer& server, const JsonObject& args){
    if(!server.GetPermissions().allowSearchDirs)throw Exc("Permission denied: Search Directories permission is required for 'ums-listdir' tool.");
    String p=args.Get("path",".").ToString();
    String ep=p;
    if(p=="."){
        if(!server.GetSandboxRoots().IsEmpty()) ep=server.GetSandboxRoots()[0];
        else {
            server.Log("Warning: ums-listdir for '.' with no sandbox roots, using current working directory.");
            ep=GetCurrentDirectory();
        }
    }
    server.EnforceSandbox(ep);
    JsonArray res; FindFile ff(AppendFileName(ep,"*.*"));
    while(ff){
        JsonObject fe;
        fe("name",ff.GetName());
        fe("is_dir",ff.IsDirectory());
        fe("is_file",ff.IsFile());
        if(ff.IsFile())fe("size",ff.GetLength());
        res.Add(fe);
        ff.Next();
    }
    server.Log("ums-listdir: Listed " + AsString(res.GetCount()) + " items in '" + ep + "'.");
    return res;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- UMS ListDir Example ---");
    McpServer server(5004,10);
    server.SetLogCallback([](const String&m){LOG("[S]: "+m);});
    server.GetPermissions().allowSearchDirs=true;
    String sandboxDir=AppendFileName(GetExeFolder(),"ums_example_sandbox_list");
    RealizeDirectory(sandboxDir); // Ensure base sandbox dir exists
    SaveFile(AppendFileName(sandboxDir,"tmp_file_for_listing.txt"),"test content"); // Create a file to list
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);

    ToolDefinition td;
    td.description="ums-listdir: Lists files and folders in a directory. Requires Search Directories permission and path must be within a sandbox root.";
    td.parameters("path",JsonObject("type","string")("optional",true)("description","Directory path to list. Defaults to first sandbox root or CWD."));
    td.func=ListDirToolLogic;

    const String toolName="ums-listdir"; // Updated tool name
    server.AddTool(toolName,td);
    server.EnableTool(toolName);
    LOG("Tool '"+toolName+"' added and enabled.");

    server.ConfigureBind(true);
    if(server.StartServer()){
        LOG("Server started on port 5004. Call tool '"+toolName+"'.");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(sandboxDir) + "\" } }");
        while(true)Sleep(1000);
    } else {
        LOG("Server start failed.");
        SetExitCode(1);
    }
}
