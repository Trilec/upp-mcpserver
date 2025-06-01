/**
 * @file Main.cpp
 * @brief Main application entry point for the U++ MCP Server.
 */

#include <CtrlLib/CtrlLib.h>
#include "../include/McpServer.h"
#include "ConfigManager.h"
#include "McpServerWindow.h"
#include <Core/Compress/Compress.h>
#include <Core/IO/FileStrm.h>
#include <Core/IO/FindFile.h>
#include <Core/IO/PathUtil.h>
#include <Core/TimeDate.h>
#include <Core/Trace/Trace.h>

// Updated PlaceholderToolLogic to use Exc for "not implemented"
JsonValue PlaceholderToolLogic(const String& toolName, McpServer& server, const JsonObject& args) {
    server.Log("Tool '" + toolName + "' called (placeholder implementation). Args: " + AsJson(args)); // Use McpServer's Log method
    throw Exc("Tool '" + toolName + "' not fully implemented yet."); // Using Exc
}

// Tool stubs now ensure they throw Exc for argument errors or permission issues.
JsonValue ReadFileTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowReadFiles) throw Exc("Permission denied: Read Files required for read_file.");
    String path = args.Get("path", "");
    if (path.IsEmpty()) throw Exc("Argument error: 'path' is required for read_file.");
    server.EnforceSandbox(path);
    // Actual LoadFile would go here in full implementation.
    return PlaceholderToolLogic("read_file", server, args); // Still calls placeholder for now
}
JsonValue CalculateTool(McpServer& server, const JsonObject& args) {
    if (args.Get("a").IsVoid()) throw Exc("Argument error: 'a' is required for calculate.");
    if (args.Get("b").IsVoid()) throw Exc("Argument error: 'b' is required for calculate.");
    if (args.Get("operation").IsVoid() || args.Get("operation").ToString().IsEmpty()) throw Exc("Argument error: 'operation' is required for calculate.");
    // Actual calculation logic here.
    return PlaceholderToolLogic("calculate", server, args);
}
JsonValue CreateDirTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowCreateDirs) throw Exc("Permission denied: Create Directories required for create_dir.");
    String path = args.Get("path", "");
    if (path.IsEmpty()) throw Exc("Argument error: 'path' is required for create_dir.");
    server.EnforceSandbox(path);
    return PlaceholderToolLogic("create_dir", server, args);
}
JsonValue ListDirTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowSearchDirs) throw Exc("Permission denied: Search Directories required for list_dir.");
    String path = args.Get("path", ".");
    server.EnforceSandbox(path);
    return PlaceholderToolLogic("list_dir", server, args);
}
JsonValue SaveDataTool(McpServer& server, const JsonObject& args) {
    if (!server.GetPermissions().allowWriteFiles) throw Exc("Permission denied: Write Files required for save_data.");
    String path = args.Get("path", "");
    if (path.IsEmpty()) throw Exc("Argument error: 'path' is required for save_data.");
    if (args.Get("data").IsVoid()) throw Exc("Argument error: 'data' is required for save_data.");
    server.EnforceSandbox(path);
    return PlaceholderToolLogic("save_data", server, args);
}

class McpApplication {
public:
    McpApplication() : mcpServer(currentConfig.serverPort) {
        installPath = GetExeFolder();
        RLOG("MCP Server starting from: " + installPath);
        cfgDir = NormalizePath(AppendFileName(installPath, "config"));
        if (!DirectoryExists(cfgDir)) RealizeDirectory(cfgDir);
        logDir = NormalizePath(AppendFileName(cfgDir, "log"));
        if (!DirectoryExists(logDir)) RealizeDirectory(logDir);
        logFilePath = NormalizePath(AppendFileName(logDir, "mcpserver.log"));
        RLOG("Log file path: " + logFilePath);
        cfgPath = NormalizePath(AppendFileName(cfgDir, "config.json"));
        if (!ConfigManager::Load(cfgPath, currentConfig)) {
            RLOG("Configuration missing or invalid (" + cfgPath + "); resetting to defaults.");
            currentConfig = Config();
            ConfigManager::Save(cfgPath, currentConfig);
            RLOG("Default configuration saved to: " + cfgPath);
        } else {
            RLOG("Configuration loaded successfully from: " + cfgPath);
        }
        mcpServer.SetPort(currentConfig.serverPort);
        mcpServer.ConfigureBind(current_config.bindAllInterfaces);

        // Setup log callback for McpServer FIRST
        mcpServer.SetLogCallback([this](const String& msg) { ProcessServerLogMessage(msg); });
        mcpServer.Log("McpApplication initialized. McpServer log callback configured."); // Now McpServer can log

        RegisterTools(); // Register tools AFTER log callback is set, so AddTool can log.

        Ctrl::Initialize();
        Ctrl::SetLanguage(LNG_ENGLISH);
        mainWindow.Create(mcpServer, currentConfig);
        mainWindow.Sizeable().Zoomable().CenterScreen();
        mainWindow.Run();
    }
    ~McpApplication() { RLOG("McpApplication shutting down."); }
private:
    void RegisterTools() {
        auto Register = [&](const String& name, const String& desc, const JsonObject& params, auto func) {
            ToolDefinition def; def.description = desc; def.parameters = params;
            def.func = [this, func, name](const JsonObject& args) { return func(mcpServer, args); };
            mcpServer.AddTool(name, def); // AddTool will log via McpServer::Log
        };
        Register("read_file", "Reads content of a file (stub). Requires Read Files.",
                 JsonObject("path", JsonObject("type","string")), ReadFileTool);
        Register("calculate", "Performs calculations (stub).",
                 JsonObject("a", JsonObject("type","number"))("b", JsonObject("type","number"))("op", JsonObject("type","string")), CalculateTool);
        Register("create_dir", "Creates a directory (stub). Requires Create Dirs.",
                 JsonObject("path", JsonObject("type","string")), CreateDirTool);
        Register("list_dir", "Lists directory contents (stub). Requires Search Dirs.",
                 JsonObject("path", JsonObject("type","string")("optional",true)), ListDirTool);
        Register("save_data", "Saves data to a file (stub). Requires Write Files.",
                 JsonObject("path", JsonObject("type","string"))("data", JsonObject("type","string")), SaveDataTool);
        // mcpServer.Log("All standard tools registered with placeholder logic."); // AddTool logs individually
    }
    void ProcessServerLogMessage(const String& msg) {
        String timestampedMsg = "[" + FormatIso8601(GetSysTime()) + "] [Server] " + msg;
        RLOG(timestampedMsg);
        if (mainWindow.IsOpen()) { mainWindow.AppendLog(msg); } // McpServerWindow.AppendLog adds its own timestamp
        FileAppend logFile(logFilePath);
        if(logFile.IsOpen()) { logFile.PutLine(timestampedMsg); logFile.Close(); }
        else { StdLog().Put("CRITICAL: Failed to open main log file for appending: " + logFilePath + "\nMessage: " + timestampedMsg + "\n");}
        int64 size = GetFileLength(logFilePath);
        if (size > (int64)currentConfig.maxLogSizeMB * 1024 * 1024 && size > 0) {
            String timeTag = FormatTime(GetSysTime(), "YYYYMMDD_HHMMSS");
            String archiveBase = NormalizePath(AppendFileName(logDir, "mcpserver_" + timeTag + ".log"));
            RLOG("Log rotation triggered. Current size: " + AsString(size) + " bytes. Max size: " + AsString(currentConfig.maxLogSizeMB) + "MB.");
            if(RenameFile(logFilePath, archiveBase)) {
                RLOG("Log file renamed to: " + archiveBase);
                String archiveGz = archiveBase + ".gz";
                if(GZCompressFile(archiveBase, archiveGz)) {
                    RLOG("Log file compressed to: " + archiveGz); DeleteFile(archiveBase);
                    if (mainWindow.IsOpen()) mainWindow.AppendLog("Log rotated to: " + archiveGz);
                } else { RLOG("Failed to compress log file: " + archiveBase); if (mainWindow.IsOpen()) mainWindow.AppendLog("Failed to compress rotated log: " + archiveBase); }
            } else { RLOG("Failed to rename log file for rotation: " + logFilePath); if (mainWindow.IsOpen()) mainWindow.AppendLog("Failed to rename log file for rotation.");}
            FileOut newLog(logFilePath);
            if(newLog.IsOpen()) { newLog.PutLine("[" + FormatIso8601(GetSysTime()) + "] [Server] Log rotated. Previous log archived (approx " + AsString(size >> 20) + "MB)."); newLog.Close(); }
        }
    }
    String installPath, cfgDir, logDir, cfgPath, logFilePath;
    Config currentConfig; McpServer mcpServer; McpServerWindow mainWindow;
};

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_FILE | LOG_TIMESTAMP | LOG_APPEND, NormalizePath(AppendFileName(GetExeFolder(), "mcpserver_startup.log")));
    SetExitCode(0);
    RLOG("Application starting...");
    McpApplication mcp_app;
    RLOG("Application main function finished. Exit code: " + AsString(GetExitCode()));
}
