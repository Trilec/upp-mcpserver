/**
 * @file Main.cpp
 * @brief Main application entry point for the U++ MCP Server.
 */

#include <CtrlLib/CtrlLib.h>     // For U++ GUI Application framework
#include "../include/McpServer.h" // For McpServer
#include "ConfigManager.h"       // For Config, ConfigManager
#include "McpServerWindow.h"     // For McpServerWindow (the main GUI)
#include <Core/Compress/Compress.h> // For GzipFile (used in log rotation)

// --- Example Tool Registration Functions (Placeholders) ---
// These would normally be more complex, possibly in their own files or part of a plugin system.

// Tool: read_file
JsonValue ReadFileTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowReadFiles) throw String("Permission denied: Read Files");
    String path = args.Get("path", "");
    if (path.IsEmpty()) throw String("Argument error: 'path' is required for read_file.");
    server.EnforceSandbox(path); // Throws on violation
    return LoadFile(path); // U++ Core function to load file content
}

// Tool: calculate
JsonValue CalculateTool(McpServer& /*server*/, const JsonObject& args) {
    double a = args.Get("a", 0.0);
    double b = args.Get("b", 0.0);
    String op = args.Get("operation", "");
    if (op == "add") return a + b;
    if (op == "subtract") return a - b;
    if (op == "multiply") return a * b;
    if (op == "divide") {
        if (b == 0) throw String("Arithmetic error: Division by zero.");
        return a / b;
    }
    throw String("Argument error: Unknown operation '") + op + "'.";
}

// Tool: create_dir
JsonValue CreateDirTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowCreateDirs) throw String("Permission denied: Create Directories");
    String path = args.Get("path", "");
    if (path.IsEmpty()) throw String("Argument error: 'path' is required for create_dir.");
    server.EnforceSandbox(GetFileFolder(path)); // Sandbox check on parent
    if (RealizeDirectory(path)) { // U++ Core, creates recursively
        return JsonValue(true);
    }
    throw String("Failed to create directory: ") + path;
}

// Tool: list_dir
JsonValue ListDirTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowSearchDirs) throw String("Permission denied: Search Directories");
    String path = args.Get("path", "."); // Defaults to current dir if not specified
    server.EnforceSandbox(path);

    JsonArray result;
    FindFile ff(AppendFileName(path, "*.*")); // List all files and dirs
    while (ff) {
        result.Add(ff.GetName());
        ff.Next();
    }
    return result;
}

// Tool: save_data
JsonValue SaveDataTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowWriteFiles) throw String("Permission denied: Write Files");
    String path = args.Get("path", "");
    String data = args.Get("data", "");
    if (path.IsEmpty()) throw String("Argument error: 'path' is required for save_data.");
    server.EnforceSandbox(path);
    if (SaveFile(path, data)) { // U++ Core function
        return JsonValue(true);
    }
    throw String("Failed to save data to file: ") + path;
}


// Main application class
class McpApplication : public TopWindow {
public:
    McpApplication() {
        // 1. Determine installPath (executable folder)
        installPath = GetExeFolder();
        LOG("MCP Server starting from: " + installPath);

        // 2. Ensure config directories exist
        cfgDir = AppendFileName(installPath, "config");
        if (!DirectoryExists(cfgDir)) RealizeDirectory(cfgDir);
        logDir = AppendFileName(cfgDir, "log");
        if (!DirectoryExists(logDir)) RealizeDirectory(logDir);
        logFilePath = AppendFileName(logDir, "mcpserver.log");

        // 3. Load or initialize configuration
        cfgPath = AppendFileName(cfgDir, "config.json");
        if (!ConfigManager::Load(cfgPath, currentConfig)) {
            LOG("Configuration missing or invalid; resetting to defaults: " + cfgPath);
            Exclamation("Configuration missing or invalid; resetting to defaults.");
            currentConfig = Config(); // Reinitialize to defaults
            ConfigManager::Save(cfgPath, currentConfig);
        } else {
            LOG("Configuration loaded successfully from: " + cfgPath);
        }

        // 4. Instantiate server with loaded settings (but don't apply all yet, GUI will handle some)
        mcpServer.SetPort(currentConfig.serverPort); // Port is fundamental for server object
        // Other settings (bind, perms, sandbox, tools) will be applied by McpServerWindow logic via currentConfig
        // when "Start Server" is clicked.

        // 5. Register all tools in code:
        RegisterTools();

        // 6. Pre-enable tools that were marked in cfg.enabledTools (server instance will be synced by GUI later)
        // This step is more about ensuring currentConfig.enabledTools is accurate if new tools were registered
        // and not in an old config file. McpServerWindow will use currentConfig to populate its UI.
        // For now, we assume config load is sufficient for currentConfig.enabledTools.

        // 7. Launch GUI window
        Ctrl::SetLanguage(LNG_ENGLISH); // Or detect system language
        mainWindow = new McpServerWindow(mcpServer, currentConfig); // Pass server and config by reference

        // 8. Bind log callback so server events appear in GUI log panel and file
        mcpServer.SetLogCallback([this](const String& msg) {
            String timestampedMsg = "[" + FormatIso8601(GetSysTime()) + "] " + msg;
            if (mainWindow && mainWindow->IsOpen()) {
                mainWindow->AppendLog(msg + "\n"); // GUI adds its own timestamp or formatting
            }

            // Append to <install>/config/log/mcpserver.log
            FileAppend out(logFilePath); // Opens in append mode
            if(out.IsOpen()) {
                out.PutLine(timestampedMsg);
                out.Close(); // Ensure data is flushed
            } else {
                RLOG("Failed to open log file for appending: " + logFilePath); // Fallback log
            }

            // If file size > cfg.maxLogSizeMB, rotate and compress
            int64 size = GetFileLength(logFilePath);
            if (size > (int64)currentConfig.maxLogSizeMB * 1024 * 1024) {
                String timeTag = Format("%04d%02d%02d_%02d%02d%02d",
                                        GetSysTime().year, GetSysTime().month, GetSysTime().day,
                                        GetSysTime().hour, GetSysTime().minute, GetSysTime().second);
                String archiveBase = AppendFileName(logDir, "mcpserver_" + timeTag + ".log");

                LOG("Log rotation triggered. Current size: " + AsString(size) + " bytes. Max size: " + AsString(currentConfig.maxLogSizeMB) + "MB.");

                if(RenameFile(logFilePath, archiveBase)) {
                    LOG("Log file renamed to: " + archiveBase);
                    String archiveGz = archiveBase + ".gz";
                    if(GZCompressFile(archiveBase, archiveGz)) { // GZCompressFile is from U++ Core/Compress.h
                        LOG("Log file compressed to: " + archiveGz);
                        DeleteFile(archiveBase); // Delete original .log after successful compression
                        if (mainWindow && mainWindow->IsOpen()) mainWindow->AppendLog("Log rotated to: " + archiveGz + "\n");
                    } else {
                        LOG("Failed to compress log file: " + archiveBase);
                        if (mainWindow && mainWindow->IsOpen()) mainWindow->AppendLog("Failed to compress rotated log: " + archiveBase + "\n");
                    }
                } else {
                     LOG("Failed to rename log file for rotation: " + logFilePath);
                     if (mainWindow && mainWindow->IsOpen()) mainWindow->AppendLog("Failed to rename log file for rotation." "\n");
                }
                // Start new log file with rotation message
                FileOut newLog(logFilePath);
                if(newLog.IsOpen()) {
                    newLog.PutLine("[" + FormatIso8601(GetSysTime()) + "] Log rotated. Previous log archived (approx " + AsString(size >> 20) + "MB).");
                    newLog.Close();
                }
            }
        });

        // Initial log message
        mcpServer.GetServer().AppendLog("Application initialized. Log system active.");


        // Set the main window for the application
        SetMainWindow(mainWindow);
        mainWindow->Sizeable().Zoomable().CenterScreen().Run();
    }

    ~McpApplication() {
        // Destructor for McpApplication
        // McpServer is a member, will be destroyed.
        // mainWindow is a pointer, should be deleted if new'd.
        // However, U++ TopWindow usually manages its own lifetime if Run() is called.
        // If mainWindow was set with SetMainWindow, U++ might handle it.
        // For safety with raw pointer: delete mainWindow; mainWindow = nullptr;
        // But if mainWindow->Run() blocks till close, it might be deleted by U++ itself.
        // Better to use UPP_HEAP with U++ GUI objects if manual new/delete is desired.
        // Or just make mainWindow a value member: McpServerWindow mainWindowInstance;
        // For now, assuming U++ handles GUI object lifetime correctly after Run().
    }

private:
    void RegisterTools() {
        // read_file
        ToolDefinition rfDef;
        rfDef.description = "Read a text file’s full contents. Requires Read Files and sandbox.";
        rfDef.parameters("path", JsonObject("type", "string")("description", "Full path to a text file."));
        rfDef.func = [this](const JsonObject& args) { return ReadFileTool(mcpServer, args); };
        mcpServer.AddTool("read_file", rfDef);

        // calculate
        ToolDefinition calcDef;
        calcDef.description = "Perform add, subtract, multiply, divide on two numbers.";
        calcDef.parameters("a", JsonObject("type", "number")("description", "First operand."))
                        ("b", JsonObject("type", "number")("description", "Second operand."))
                        ("operation", JsonObject("type", "string")("description", "\"add\"|\"subtract\"|\"multiply\"|\"divide\""));
        calcDef.func = [this](const JsonObject& args) { return CalculateTool(mcpServer, args); };
        mcpServer.AddTool("calculate", calcDef);

        // create_dir
        ToolDefinition cdDef;
        cdDef.description = "Create directory at specified path. Requires Create Directories and sandbox.";
        cdDef.parameters("path", JsonObject("type", "string")("description", "Full path for new folder."));
        cdDef.func = [this](const JsonObject& args) { return CreateDirTool(mcpServer, args); };
        mcpServer.AddTool("create_dir", cdDef);

        // list_dir
        ToolDefinition ldDef;
        ldDef.description = "List files and folders in a directory. Requires Search Directories and sandbox.";
        ldDef.parameters("path", JsonObject("type", "string")("optional", true)("description", "Directory path (defaults to current)."));
        ldDef.func = [this](const JsonObject& args) { return ListDirTool(mcpServer, args); };
        mcpServer.AddTool("list_dir", ldDef);

        // save_data
        ToolDefinition sdDef;
        sdDef.description = "Save text to file at given path. Requires Write Files and sandbox.";
        sdDef.parameters("path", JsonObject("type", "string")("description", "Full file path."))
                        ("data", JsonObject("type", "string")("description", "Text content to write."));
        sdDef.func = [this](const JsonObject& args) { return SaveDataTool(mcpServer, args); };
        mcpServer.AddTool("save_data", sdDef);

        mcpServer.GetServer().AppendLog("All standard tools registered.");
    }

    String installPath;
    String cfgDir;
    String logDir;
    String cfgPath;
    String logFilePath;

    Config currentConfig;
    McpServer mcpServer; // Server instance (port is initially from config, can be changed by GUI)
    McpServerWindow *mainWindow; // Main GUI window - pointer to allow U++ to manage its creation/lifetime
};

// U++ GUI applications typically use GUI_APP_MAIN
// CONSOLE_APP_MAIN is for console apps or GUI apps that might need a console.
// The design brief says "CONSOLE_APP_MAIN" for src/Main.cpp.
CONSOLE_APP_MAIN {
    StdLogSetup(LOG_FILE | LOG_TIMESTAMP | LOG_APPEND, AppendFileName(GetExeFolder(), "debug_main.log")); // For early debug logs
    SetExitCode(0); // Default exit code

    // U++ apps often have a single TopWindow derived class that is the app.
    McpApplication mcp_app;
    // mcp_app.Run(); // If McpApplication itself is the TopWindow to run.
    // The current structure has McpApplication create McpServerWindow and Run() it.
    // This is fine. McpApplication's constructor does all the work.
    // If McpApplication is not a TopWindow itself, it doesn't need to Run().
    // The current McpApplication constructor calls mainWindow->Run() which blocks.
    // So, McpApplication object `mcp_app` is created, its constructor runs and blocks.
    // This is a valid way to structure it.
    LOG("McpApplication object created, constructor has completed or is blocked by Run().");
    LOG("Application exiting with code: " + AsString(GetExitCode()));
}
