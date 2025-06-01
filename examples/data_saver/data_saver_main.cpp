/**
 * @file data_saver_main.cpp
 * @brief Example plugin: save_data tool.
 */

#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

// Tool logic for save_data
JsonValue SaveDataToolLogic(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowWriteFiles) {
        throw String("Permission denied: Write Files is not enabled for the server instance.");
    }

    String path = args.Get("path", "");
    if (path.IsEmpty()) {
        throw String("Argument error: 'path' is required for save_data.");
    }

    String data = args.Get("data", "");
    // Allow empty string data, but "data" field should exist if tool spec requires it.
    // Current spec in brief: "data": { "type":"string", "description":"Text content to write." }
    // This implies "data" is not optional.
    if (args.Get("data").IsVoid()) {
         throw String("Argument error: 'data' field is required for save_data.");
    }


    server.EnforceSandbox(path); // Path must be within a sandbox root

    if (SaveFile(path, data)) { // U++ Core function, overwrites if file exists
        return JsonValue(true); // Success
    } else {
        throw String("File system error: Failed to save data to file '") + path + "'. Check path and permissions.";
    }
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    SetExitCode(0);

    LOG("--- Data Saver (save_data) Example Plugin ---");

    int port = 5005; // Different port
    McpServer server(port, 10);
    LOG("McpServer instance created for save_data_example on port " + AsString(port));

    server.SetLogCallback([](const String& msg) {
        LOG("[Server]: " + msg);
    });

    server.GetPermissions().allowWriteFiles = true;
    LOG("Enabled 'allowWriteFiles' permission.");

    String sandboxDir = AppendFileName(GetExeFolder(), "example_sandbox_savetest");
    RealizeDirectory(sandboxDir); // Ensure the sandbox directory itself exists
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);
    LOG("The tool will be able to save files INSIDE this root, e.g., '" + AppendFileName(sandboxDir, "my_data.txt") + "'.");

    ToolDefinition td;
    td.description = "Save text data to a file at the specified path. Requires Write Files permission and path must be within a sandbox root.";

    JsonObject params;
    JsonObject pathParam, dataParam;
    pathParam("type", "string");
    pathParam("description", "Full file path to save the data (e.g., " + AppendFileName(sandboxDir, "output.txt") + ").");
    params("path", pathParam);

    dataParam("type", "string");
    dataParam("description", "Text content to write to the file.");
    params("data", dataParam);

    td.parameters = params;

    td.func = [&](const JsonObject& args) -> JsonValue {
        return SaveDataToolLogic(server, args);
    };

    const String toolName = "save_data_example";
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '" + toolName + "' added and enabled.");

    server.ConfigureBind(true);
    if (server.StartServer()) {
        LOG("Server started. Connect a WebSocket client to ws://localhost:" + AsString(port));
        String exampleFilePath = AppendFileName(sandboxDir, "test_save.json");
        String exampleData = "{ \"message\": \"Hello from data_saver!\" }";
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(exampleFilePath) + "\", \"data\": \"" + EscapeJSON(exampleData) + "\" } }");
        LOG("After calling, check for file: " + exampleFilePath);
        while (true) {
            Sleep(1000);
        }
    } else {
        LOG("Failed to start the server on port " + AsString(port));
        SetExitCode(1);
    }
}
