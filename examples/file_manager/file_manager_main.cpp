/**
 * @file file_manager_main.cpp
 * @brief Example plugin: create_dir tool.
 */

#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

// Tool logic for create_dir
JsonValue CreateDirToolLogic(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowCreateDirs) {
        throw String("Permission denied: Create Directories is not enabled for the server instance.");
    }

    String path = args.Get("path", "");
    if (path.IsEmpty()) {
        throw String("Argument error: 'path' is required for create_dir.");
    }

    // Sandbox check should be on the path being created or its parent.
    // If path is "C:/sandbox/newA/newB", sandbox root "C:/sandbox/" should allow this.
    // EnforceSandbox will check if 'path' is under a root.
    // For creating, it's crucial that the *intended location* is sandboxed.
    server.EnforceSandbox(path);

    if (RealizeDirectory(path)) { // U++ Core function, creates recursively
        return JsonValue(true); // Success
    } else {
        // Check if it already exists and is a directory
        if (DirectoryExists(path)) {
             return JsonValue(true); // Idempotency: already exists, consider it success.
        }
        throw String("File system error: Failed to create directory '") + path + "'. It might be a file or uncreatable.";
    }
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    SetExitCode(0);

    LOG("--- File Manager (create_dir) Example Plugin ---");

    int port = 5003; // Different port
    McpServer server(port, 10);
    LOG("McpServer instance created for create_dir_example on port " + AsString(port));

    server.SetLogCallback([](const String& msg) {
        LOG("[Server]: " + msg);
    });

    server.GetPermissions().allowCreateDirs = true;
    LOG("Enabled 'allowCreateDirs' permission.");

    String sandboxDir = AppendFileName(GetExeFolder(), "example_sandbox_createtest");
    // We don't necessarily create sandboxDir itself here,
    // RealizeDirectory in the tool will create subdirs within it.
    // But the root itself must exist for AddSandboxRoot to be meaningful for some checks.
    RealizeDirectory(sandboxDir);
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir);
    LOG("The tool will be able to create directories INSIDE this root, e.g., '" + AppendFileName(sandboxDir, "my_new_folder") + "'.");

    ToolDefinition td;
    td.description = "Create a directory (and any necessary parent directories) at the specified path. Requires Create Directories permission and path must be within a sandbox root.";

    JsonObject params;
    JsonObject pathParam;
    pathParam("type", "string");
    pathParam("description", "Full path for the new folder (e.g., " + AppendFileName(sandboxDir, "new_folder/sub_folder") + ").");
    params("path", pathParam);
    td.parameters = params;

    td.func = [&](const JsonObject& args) -> JsonValue {
        return CreateDirToolLogic(server, args);
    };

    const String toolName = "create_dir_example";
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '" + toolName + "' added and enabled.");

    server.ConfigureBind(true);
    if (server.StartServer()) {
        LOG("Server started. Connect a WebSocket client to ws://localhost:" + AsString(port));
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(AppendFileName(sandboxDir, "my_new_folder")) + "\" } }");
        while (true) {
            Sleep(1000);
        }
    } else {
        LOG("Failed to start the server on port " + AsString(port));
        SetExitCode(1);
    }
}
