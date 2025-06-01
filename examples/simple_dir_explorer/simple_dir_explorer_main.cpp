/**
 * @file simple_dir_explorer_main.cpp
 * @brief Example plugin: list_dir tool.
 */

#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Json/Json.h>

// Tool logic for list_dir
JsonValue ListDirToolLogic(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowSearchDirs) {
        throw String("Permission denied: Search Directories is not enabled for the server instance.");
    }

    String path = args.Get("path", "."); // Defaults to current dir if not specified
    // If path is relative, it's relative to the server's CWD.
    // For robustness, tools might want to resolve to absolute paths or clarify behavior.
    // For now, assume path is either absolute or resolvable by server.

    String effectivePath = path;
    if (path == ".") { // Example: resolve "." to one of the sandbox roots or a default sub-sandbox
        if (server.GetSandboxRoots().GetCount() > 0) {
            effectivePath = server.GetSandboxRoots()[0]; // Default to first sandbox root
        } else {
            // Or use GetCurrentDirectory() if no sandbox, but that's risky.
            // For this example, if path is "." and no sandbox, it will try CWD.
            effectivePath = GetCurrentDirectory();
        }
    }

    server.EnforceSandbox(effectivePath);

    JsonArray result;
    FindFile ff(AppendFileName(effectivePath, "*.*")); // List all files and dirs
    while (ff) {
        JsonObject fileEntry;
        fileEntry("name", ff.GetName());
        fileEntry("is_dir", ff.IsDirectory());
        fileEntry("is_file", ff.IsFile());
        fileEntry("size", ff.GetLength()); // Length for files
        // fileEntry("last_modified", FormatIso8601(ff.GetLastWriteTime())); // More detailed info
        result.Add(fileEntry);
        ff.Next();
    }
    return result;
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    SetExitCode(0);

    LOG("--- Simple Dir Explorer (list_dir) Example Plugin ---");

    int port = 5004; // Different port
    McpServer server(port, 10);
    LOG("McpServer instance created for list_dir_example on port " + AsString(port));

    server.SetLogCallback([](const String& msg) {
        LOG("[Server]: " + msg);
    });

    server.GetPermissions().allowSearchDirs = true;
    LOG("Enabled 'allowSearchDirs' permission.");

    String sandboxDir = AppendFileName(GetExeFolder(), "example_sandbox_listtest");
    RealizeDirectory(AppendFileName(sandboxDir, "subdir1")); // Create some structure
    SaveFile(AppendFileName(sandboxDir, "file1.txt"), "Test file 1");
    SaveFile(AppendFileName(AppendFileName(sandboxDir, "subdir1"), "file2.txt"), "Test file 2 in subdir1");
    server.AddSandboxRoot(sandboxDir);
    LOG("Added sandbox root: " + sandboxDir + " (with some initial content)");

    ToolDefinition td;
    td.description = "List files and folders in a directory. Requires Search Directories permission and path must be within a sandbox root.";

    JsonObject params;
    JsonObject pathParam;
    pathParam("type", "string");
    pathParam("optional", true);
    pathParam("description", "Directory path to list (e.g., '" + sandboxDir + "' or './subdir1' if current dir is sandboxDir). Defaults to '.' (first sandbox root or server CWD).");
    params("path", pathParam);
    td.parameters = params;

    td.func = [&](const JsonObject& args) -> JsonValue {
        return ListDirToolLogic(server, args);
    };

    const String toolName = "list_dir_example";
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '" + toolName + "' added and enabled.");

    server.ConfigureBind(true);
    if (server.StartServer()) {
        LOG("Server started. Connect a WebSocket client to ws://localhost:" + AsString(port));
        LOG("Example tool call (listing sandbox root): { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"" + EscapeJSON(sandboxDir) + "\" } }");
        LOG("Example tool call (listing relative to sandbox root, if CWD is sandbox root): { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"path\": \"subdir1\" } }");
        LOG("Example tool call (defaulting to '.'): { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\" }");
        while (true) {
            Sleep(1000);
        }
    } else {
        LOG("Failed to start the server on port " + AsString(port));
        SetExitCode(1);
    }
}
