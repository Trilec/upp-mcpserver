/**
 * @file Main.cpp
 * @brief Main application entry point for the U++ MCP Server.
 */

#include <CtrlLib/CtrlLib.h>     // For U++ GUI Application framework
#include "../include/McpServer.h" // For McpServer
#include "ConfigManager.h"       // For Config, ConfigManager
#include "McpServerWindow.h"     // For McpServerWindow (the main GUI)

// U++ includes for file operations and compression
#include <Core/Compress/Compress.h> // For GZCompressFile
#include <Core/IO/FileStrm.h>       // For FileIn, FileOut, FileAppend
#include <Core/IO/FindFile.h>       // For DirectoryExists, GetFileLength, DeleteFile, RenameFile
#include <Core/IO/PathUtil.h>       // For GetExeFolder, AppendFileName, RealizeDirectory, GetFileFolder
#include <Core/TimeDate.h>          // For GetSysTime, FormatIso8601
#include <Core/Trace/Trace.h>       // For LOG, StdLogSetup, RLOG

// --- Minimal McpServer for GUI interaction phase ---
// This version of McpServer will be enhanced in later backend-focused steps.
// For now, it needs to:
// - Hold basic config state (port, bindAllInterfaces).
// - Manage a list of registered tools (name, description, params).
// - Simulate Start/Stop and maintain IsListening status.
// - Provide SetLogCallback and AppendLog.

// The McpServer class is defined in include/McpServer.h and implemented in src/McpServer.cpp
// We will rely on that implementation but assume for this phase its Start/Stop are minimal.
// The McpServer::AppendLog method is not standard in the current McpServer.h, it uses logCallback.
// We will use the logCallback mechanism.

// --- Example Tool Registration Stubs ---
// These provide metadata for tools the GUI will list.
// The actual tool logic can remain basic for now.

JsonValue PlaceholderToolLogic(const String& toolName, McpServer& server, const JsonObject& args) {
    // server.GetServer().AppendLog("Tool '" + toolName + "' called (placeholder implementation). Args: " + AsJson(args));
    // To use server.AppendLog, McpServer needs such a method, or use the logCallback.
    // The existing McpServer uses logCallback. So we'd call that if available.
	if(server.logCallback) server.logCallback("Tool '" + toolName + "' called (placeholder implementation). Args: " + AsJson(args));
    // For now, just return a dummy response.
    // throw Exc("Tool '" + toolName + "' not fully implemented yet.");
    JsonObject result;
    result("status", "Tool " + toolName + " called, but not fully implemented.");
    result("received_args", args);
    return result;
}

// Tool: read_file (stub)
JsonValue ReadFileTool(McpServer& server, const JsonObject& args) {
    // Actual logic for read_file was in example, here it's simplified for main app registration
    // server.GetPermissions().allowReadFiles, server.EnforceSandbox(path), LoadFile(path)
    return PlaceholderToolLogic("read_file", server, args);
}
// Tool: calculate (stub)
JsonValue CalculateTool(McpServer& server, const JsonObject& args) {
    return PlaceholderToolLogic("calculate", server, args);
}
// Tool: create_dir (stub)
JsonValue CreateDirTool(McpServer& server, const JsonObject& args) {
    return PlaceholderToolLogic("create_dir", server, args);
}
// Tool: list_dir (stub)
JsonValue ListDirTool(McpServer& server, const JsonObject& args) {
    return PlaceholderToolLogic("list_dir", server, args);
}
// Tool: save_data (stub)
JsonValue SaveDataTool(McpServer& server, const JsonObject& args) {
    return PlaceholderToolLogic("save_data", server, args);
}


// Main application class
class McpApplication { // Not a TopWindow itself, just manages setup and owns objects
public:
    McpApplication() : mcpServer(currentConfig.serverPort) { // Initialize McpServer with a port from potentially default config
        // 1. Determine installPath (executable folder)
        installPath = GetExeFolder();
        RLOG("MCP Server starting from: " + installPath); // Use RLOG for early critical logs if LOG isn't set up

        // 2. Ensure config directories exist
        cfgDir = NormalizePath(AppendFileName(installPath, "config"));
        if (!DirectoryExists(cfgDir)) RealizeDirectory(cfgDir);
        logDir = NormalizePath(AppendFileName(cfgDir, "log"));
        if (!DirectoryExists(logDir)) RealizeDirectory(logDir);

        // Log file path setup (used by log callback)
        logFilePath = NormalizePath(AppendFileName(logDir, "mcpserver.log"));
        RLOG("Log file path: " + logFilePath);

        // 3. Load or initialize configuration
        cfgPath = NormalizePath(AppendFileName(cfgDir, "config.json"));
        if (!ConfigManager::Load(cfgPath, currentConfig)) {
            RLOG("Configuration missing or invalid (" + cfgPath + "); resetting to defaults.");
            // Prompt user or log extensively. For now, just log and save defaults.
            // Exclamation("Configuration missing or invalid; resetting to defaults."); // Requires GUI context
            currentConfig = Config(); // Reinitialize to defaults
            ConfigManager::Save(cfgPath, currentConfig); // Save defaults
            RLOG("Default configuration saved to: " + cfgPath);
        } else {
            RLOG("Configuration loaded successfully from: " + cfgPath);
        }

        // Apply initial port from loaded/default config to McpServer instance
        // McpServer constructor already took currentConfig.serverPort which was default at that time.
        // If config loaded a different port, update the server instance.
        mcpServer.SetPort(currentConfig.serverPort);
        mcpServer.ConfigureBind(currentConfig.bindAllInterfaces); // Also apply bind preference

        // 4. Register all tools in code (these are for the main application's server instance):
        RegisterTools();

        // 5. Setup log callback for McpServer (critical to do this early)
        // This routes server's internal logs to our file logger and potentially GUI
        mcpServer.SetLogCallback([this](const String& msg) {
            ProcessServerLogMessage(msg);
        });

        // Initial log message via the server's mechanism
        // mcpServer.GetServer().AppendLog("McpApplication initialized. McpServer log callback configured.");
        // Note: McpServer.h needs 'McpServer& GetServer() { return *this; }' for AppendLog to be chainable like this.
        // Or just mcpServer.AppendLog("..."). For now, using the logCallback for all server messages.
        // The above AppendLog implies McpServer has this public method. Let's assume it does for now,
        // or it's a helper that uses the logCallback. The current McpServer.cpp doesn't have public AppendLog.
        // It's better if McpServer uses its own logCallback to emit messages.
        // So, internal server actions should call `if(logCallback) logCallback("message");`
		if(mcpServer.logCallback) mcpServer.logCallback("McpApplication initialized. McpServer log callback configured.");


        // 6. Launch GUI window
        Ctrl::Initialize(); // Ensure U++ GUI is initialized
        Ctrl::SetLanguage(LNG_ENGLISH);

        // McpServerWindow takes references to the server and its config.
        // The config currentConfig is owned by McpApplication.
        // The mcpServer is also owned by McpApplication.
        mainWindow.Create(mcpServer, currentConfig); // Create instance, but don't Run yet if more setup

        // Route server log messages (captured by ProcessServerLogMessage) to the GUI window's log display
        // This is done by ProcessServerLogMessage calling mainWindow.AppendLog

        // Finalize window setup and run
        mainWindow.Sizeable().Zoomable().CenterScreen();
        mainWindow.Run(); // This blocks until the main window is closed.
    }

    ~McpApplication() {
        RLOG("McpApplication shutting down.");
        // mainWindow is an object member, its destructor will be called.
        // mcpServer is an object member, its destructor will be called (should stop server if running).
    }

private:
    void RegisterTools() {
        // Simplified tool registration for the main application's server instance.
        // These use the placeholder logic defined at the top of this file.
        auto Register = [&](const String& name, const String& desc, const JsonObject& params, auto func) {
            ToolDefinition def;
            def.description = desc;
            def.parameters = params;
            def.func = [this, func, name](const JsonObject& args) { return func(mcpServer, args); };
            mcpServer.AddTool(name, def);
        };

        Register("read_file", "Reads content of a file (stub).",
                 JsonObject("path", JsonObject("type","string")), ReadFileTool);
        Register("calculate", "Performs calculations (stub).",
                 JsonObject("a", JsonObject("type","number"))("b", JsonObject("type","number"))("op", JsonObject("type","string")), CalculateTool);
        Register("create_dir", "Creates a directory (stub).",
                 JsonObject("path", JsonObject("type","string")), CreateDirTool);
        Register("list_dir", "Lists directory contents (stub).",
                 JsonObject("path", JsonObject("type","string")("optional",true)), ListDirTool);
        Register("save_data", "Saves data to a file (stub).",
                 JsonObject("path", JsonObject("type","string"))("data", JsonObject("type","string")), SaveDataTool);

        if(mcpServer.logCallback) mcpServer.logCallback("All standard tools registered with placeholder logic for main server instance.");
    }

    void ProcessServerLogMessage(const String& msg) {
        String timestampedMsg = "[" + FormatIso8601(GetSysTime()) + "] [Server] " + msg;

        // Log to console (if debugging or if RLOG/LOG points there)
        RLOG(timestampedMsg); // RLOG is good for raw log, might go to stderr or file depending on StdLogSetup

        // Append to GUI log panel if window is open
        if (mainWindow.IsOpen()) {
            // mainWindow.AppendLog needs to be callable.
            // It might need to be deferred if logging from non-GUI thread.
            // For now, assume direct call is okay or AppendLog handles threading.
            mainWindow.AppendLog(msg); // GUI window might add its own timestamp or formatting
        }

        // Append to rolling log file
        FileAppend logFile(logFilePath);
        if(logFile.IsOpen()) {
            logFile.PutLine(timestampedMsg);
            logFile.Close(); // Ensure data is flushed
        } else {
            // Fallback if main log file fails
            StdLog().Put("CRITICAL: Failed to open main log file for appending: " + logFilePath + "\nMessage: " + timestampedMsg + "\n");
        }

        // Log Rotation Check (moved here to centralize file logging)
        // This should ideally be less frequent or more robust than checking on every message.
        // For now, keeping it simple.
        int64 size = GetFileLength(logFilePath);
        if (size > (int64)currentConfig.maxLogSizeMB * 1024 * 1024 && size > 0) { // size > 0 to avoid issues with non-existent file
            String timeTag = FormatTime(GetSysTime(), "YYYYMMDD_HHMMSS");
            String archiveBase = NormalizePath(AppendFileName(logDir, "mcpserver_" + timeTag + ".log"));

            RLOG("Log rotation triggered. Current size: " + AsString(size) + " bytes. Max size: " + AsString(currentConfig.maxLogSizeMB) + "MB.");

            if(RenameFile(logFilePath, archiveBase)) {
                RLOG("Log file renamed to: " + archiveBase);
                String archiveGz = archiveBase + ".gz";
                if(GZCompressFile(archiveBase, archiveGz)) {
                    RLOG("Log file compressed to: " + archiveGz);
                    DeleteFile(archiveBase);
                    if (mainWindow.IsOpen()) mainWindow.AppendLog("Log rotated to: " + archiveGz);
                } else {
                    RLOG("Failed to compress log file: " + archiveBase);
                    if (mainWindow.IsOpen()) mainWindow.AppendLog("Failed to compress rotated log: " + archiveBase);
                }
            } else {
                 RLOG("Failed to rename log file for rotation: " + logFilePath);
                 if (mainWindow.IsOpen()) mainWindow.AppendLog("Failed to rename log file for rotation.");
            }
            // Start new log file with rotation message
            FileOut newLog(logFilePath); // This creates/truncates
            if(newLog.IsOpen()) {
                newLog.PutLine("[" + FormatIso8601(GetSysTime()) + "] [Server] Log rotated. Previous log archived (approx " + AsString(size >> 20) + "MB).");
                newLog.Close();
            }
        }
    }

    String installPath;
    String cfgDir;
    String logDir;
    String cfgPath;
    String logFilePath;

    Config currentConfig; // Owns the application's current configuration
    McpServer mcpServer;  // Owns the server instance
    McpServerWindow mainWindow; // Owns the main GUI window object
};

// U++ GUI applications typically use GUI_APP_MAIN
// The design brief specified CONSOLE_APP_MAIN for src/Main.cpp.
// CONSOLE_APP_MAIN will show a console window along with the GUI if not detached.
// GUI_APP_MAIN is usually preferred for pure GUI apps.
// Let's stick to CONSOLE_APP_MAIN as per original design doc's specific instruction.
CONSOLE_APP_MAIN {
    // Setup a basic fallback log early for any issues during McpApplication construction.
    // This could go to a file like "debug_main.log" or console.
    StdLogSetup(LOG_FILE | LOG_TIMESTAMP | LOG_APPEND, NormalizePath(AppendFileName(GetExeFolder(), "mcpserver_startup.log")));
    SetExitCode(0); // Default exit code

    RLOG("Application starting...");

    McpApplication mcp_app; // Creates and runs the application in its constructor

    RLOG("Application main function finished (mainWindow.Run() has returned). Exit code: " + AsString(GetExitCode()));
}
