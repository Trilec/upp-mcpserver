/**
 * @file file_reader_main.cpp
 * @brief Example plugin: read_file tool.
 *
 * This console application demonstrates how to register and run
 * the "read_file" tool with an McpServer instance.
 */

#include "../../include/McpServer.h" // Path to McpServer.h from examples/file_reader/
#include <Core/Core.h>             // For U++ Core utilities like LOG, SetExitCode
#include <Json/Json.h>             // For JsonObject, JsonValue

// Tool logic for read_file
JsonValue ReadFileToolLogic(McpServer& server, const JsonObject& args) {
    // Check permission (server instance should have it configured)
    if (!server.GetPermissions().allowReadFiles) {
        throw String("Permission denied: Read Files is not enabled for the server instance.");
    }

    String path = args.Get("path", "");
    if (path.IsEmpty()) {
        throw String("Argument error: 'path' is required for read_file.");
    }

    // Enforce sandbox (server instance should have roots configured)
    server.EnforceSandbox(path); // Throws on violation

    String content = LoadFile(path); // U++ Core function
    if (content.IsVoid()) { // LoadFile returns void String on failure
        throw String("File error: Could not read file '") + path + "'.";
    }
    return content;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); // Log to console for simplicity
    SetExitCode(0);

    LOG("--- File Reader Example Plugin ---");

    // 1. Instantiate McpServer
    int port = 5001; // Use a different port than the main app for standalone testing
    McpServer server(port, 10);
    LOG("McpServer instance created for file_reader_example on port " + AsString(port));

    // Setup log callback for the server instance
    server.SetLogCallback([](const String& msg) {
        LOG("[Server]: " + msg);
    });

    // 2. Configure server instance for this tool (permissions, sandbox)
    // For this example, we enable necessary permissions and set a sandbox root.
    // In a real scenario, these would be loaded from a config or set by a user.
    server.GetPermissions().allowReadFiles = true;
    LOG("Enabled 'allowReadFiles' permission for this server instance.");

    String sandboxDir = AppendFileName(GetExeFolder(), "example_sandbox");
    RealizeDirectory(sandboxDir); // Ensure it exists
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);
    LOG("Create a file like '" + AppendFileName(sandboxDir, "test.txt") + "' to test.");
    SaveFile(AppendFileName(sandboxDir, "test.txt"), "Hello from file_reader example!");


    // 3. Define ToolDefinition
    ToolDefinition td;
    td.description = "Read a text file’s full contents. Requires Read Files permission and path to be within a configured sandbox root.";

    JsonObject params;
    JsonObject pathParam;
    pathParam("type", "string");
    pathParam("description", "Full path to a text file (e.g., " + AppendFileName(sandboxDir,"test.txt") + ").");
    params("path", pathParam);
    td.parameters = params;

    // Bind the tool logic function. Pass the server instance if the logic needs it.
    td.func = [&](const JsonObject& args) -> JsonValue {
        return ReadFileToolLogic(server, args);
    };
    // Or, if ReadFileToolLogic is a static/global function that takes McpServer& as first arg:
    // td.func = std.bind(&ReadFileToolLogic, std::ref(server), std::placeholders::_1);

    // 4. Add and Enable Tool
    const String toolName = "read_file_example"; // Give it a distinct name for the example
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '" + toolName + "' added and enabled.");

    // 5. Start Server
    server.ConfigureBind(true); // Bind to all interfaces for easier testing
    if (server.StartServer()) {
        LOG("Server started. Connect a WebSocket client to ws://localhost:" + AsString(port));
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(AppendFileName(sandboxDir,"test.txt")) + "\" } }");
        // Loop indefinitely until Ctrl+C or process is killed
        while (true) {
            Sleep(1000); // Keep main thread alive
            // In a real app, might have server.Poll() or other event loop
        }
    } else {
        LOG("Failed to start the server on port " + AsString(port));
        SetExitCode(1);
    }
}
