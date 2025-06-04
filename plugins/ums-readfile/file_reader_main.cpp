#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Core/Json.h> // For JsonObject, JsonArray, Value (though Core.h might cover some)
#include <Core/ValueUtil.h> // For AsJson for logging (if used, EscapeJSON is from Core/Core.h)

JsonValue ReadFileToolLogic(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowReadFiles) throw Exc("Permission denied: Read Files permission is required for 'ums-readfile' tool.");
    String path = args.Get("path", "").ToString();
    if (path.IsEmpty()) throw Exc("Argument error: 'path' is a required string argument for 'ums-readfile' tool.");
    server.EnforceSandbox(path);
    String content = LoadFile(path);
    if (content.IsVoid()) throw Exc("File error: Could not read file '" + path + "'.");
    return content;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- UMS File Reader Example ---");
    McpServer server(5001, 10);
    server.SetLogCallback([](const String&m){LOG("[S]: "+m);});
    server.GetPermissions().allowReadFiles = true;
    String sandboxDir = AppendFileName(GetExeFolder(), "ums_example_sandbox");
    RealizeDirectory(sandboxDir);
    server.AddSandboxRoot(sandboxDir);
    String testFilePath = AppendFileName(sandboxDir,"test.txt");
    SaveFile(testFilePath,"Hello from ums-readfile example!");
    LOG("Test file created at: " + testFilePath);

    ToolDefinition td;
    td.description = "ums-readfile: Reads a text file’s full contents. Requires Read Files permission and path to be within a configured sandbox root.";
    td.parameters("path",JsonObject("type","string")("description","Full path to a text file."));
    td.func = [&](const JsonObject&a){return ReadFileToolLogic(server,a);};

    const String toolName = "ums-readfile"; // Updated tool name
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '"+toolName+"' added and enabled.");

    server.ConfigureBind(true);
    if(server.StartServer()){
        LOG("Server started on port 5001. Call tool '"+toolName+"'.");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(testFilePath) + "\" } }");
        while(true)Sleep(1000);
    } else {
        LOG("Server start failed.");
        SetExitCode(1);
    }
}
