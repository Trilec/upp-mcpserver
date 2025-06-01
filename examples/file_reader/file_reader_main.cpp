#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

JsonValue ReadFileToolLogic(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowReadFiles) {
        throw Exc("Permission denied: Read Files is not enabled for the server instance."); // Use Exc
    }
    String path = args.Get("path", "");
    if (path.IsEmpty()) {
        throw Exc("Argument error: 'path' is required for read_file."); // Use Exc
    }
    server.EnforceSandbox(path);
    String content = LoadFile(path);
    if (content.IsVoid()) {
        throw Exc("File error: Could not read file '") + path + "'."; // Use Exc
    }
    return content;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- File Reader Example Plugin ---");
    int port = 5001;
    McpServer server(port, 10);
    LOG("McpServer instance created for file_reader_example on port " + AsString(port));
    server.SetLogCallback([](const String& msg) { LOG("[Server]: " + msg); }); // Server instance logs to console
    server.GetPermissions().allowReadFiles = true;
    LOG("Enabled 'allowReadFiles' permission for this server instance.");
    String sandboxDir = AppendFileName(GetExeFolder(), "example_sandbox");
    RealizeDirectory(sandboxDir);
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);
    // Create a test file in the sandbox for the example to read
    String testFilePath = AppendFileName(sandboxDir,"test.txt");
    SaveFile(testFilePath, "Hello from file_reader example!");
    LOG("Created test file: " + testFilePath);

    ToolDefinition td;
    td.description = "Read a text file’s full contents. Requires Read Files permission and path to be within a configured sandbox root.";
    JsonObject params; JsonObject pathParam;
    pathParam("type", "string"); pathParam("description", "Full path to a text file (e.g., " + EscapeJSON(testFilePath) + ").");
    params("path", pathParam); td.parameters = params;
    td.func = [&](const JsonObject& args) -> JsonValue { return ReadFileToolLogic(server, args); };
    const String toolName = "read_file_example";
    server.AddTool(toolName, td); // AddTool will log through server's Log method -> console
    server.EnableTool(toolName);  // EnableTool will also log

    server.ConfigureBind(true);
    if (server.StartServer()) { // StartServer will log
        LOG("Server started. Connect client to ws://localhost:" + AsString(port));
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(testFilePath) + "\" } }");
        while (true) { Sleep(1000); }
    } else {
        LOG("Failed to start server on port " + AsString(port));
        SetExitCode(1);
    }
}
