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
#include <Core/ValueUtil.h>

// Tool function implementations (names like ReadFileTool remain for the C++ functions)
Value ReadFileTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-readfile (ReadFileTool) invoked. Args: " + AsJson(args));
    if (!server.GetPermissions().allowReadFiles) {
        throw Exc("Permission denied: Read Files permission is required for 'ums-readfile' tool.");
    }
    String path = args.Get("path", "").ToString();
    if (path.IsEmpty()) {
        throw Exc("Argument error: 'path' is a required string argument for 'ums-readfile' tool.");
    }
    server.EnforceSandbox(path);
    String content = LoadFile(path);
    if (content.IsVoid()) {
        throw Exc("File operation error: Could not read file '" + path + "'. Ensure file exists and is accessible.");
    }
    server.Log("ums-readfile success for path: " + path);
    return content;
}

Value CalculateTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-calc (CalculateTool) invoked. Args: " + AsJson(args));
    Value val_a = args.Get("a"); Value val_b = args.Get("b");
    String operation = args.Get("operation", "").ToString();
    if (!val_a.IsNumber()) throw Exc("Argument error: 'a' must be a number for 'ums-calc' tool."); // IsNumber is more general
    if (!val_b.IsNumber()) throw Exc("Argument error: 'b' must be a number for 'ums-calc' tool.");
    if (operation.IsEmpty()) throw Exc("Argument error: 'operation' (string: add, subtract, multiply, divide) is required for 'ums-calc' tool.");
    double a = val_a.To<double>(); double b = val_b.To<double>();
    if (operation == "add") return a + b; if (operation == "subtract") return a - b;
    if (operation == "multiply") return a * b;
    if (operation == "divide") {
        if (b == 0) throw Exc("Arithmetic error: Division by zero in 'ums-calc' tool.");
        return a / b;
    }
    throw Exc("Argument error: Unknown operation '" + operation + "'. Supported: add, subtract, multiply, divide.");
}

Value CreateDirTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-createdir (CreateDirTool) invoked. Args: " + AsJson(args));
    if (!server.GetPermissions().allowCreateDirs) {
        throw Exc("Permission denied: Create Directories permission is required for 'ums-createdir' tool.");
    }
    String path = args.Get("path", "").ToString();
    if (path.IsEmpty()) {
        throw Exc("Argument error: 'path' is a required string argument for 'ums-createdir' tool.");
    }
    server.EnforceSandbox(path);
    if (DirectoryExists(path)) {
        server.Log("ums-createdir: Directory '" + path + "' already exists."); return true;
    }
    if (!RealizeDirectory(path)) {
        throw Exc("File system error: Failed to create directory '" + path + "'. Check path validity and OS permissions.");
    }
    server.Log("ums-createdir success: Directory '" + path + "' created.");
    return true;
}

Value ListDirTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-listdir (ListDirTool) invoked. Args: " + AsJson(args));
    if (!server.GetPermissions().allowSearchDirs) {
        throw Exc("Permission denied: Search Directories permission is required for 'ums-listdir' tool.");
    }
    String path_arg = args.Get("path", ".").ToString();
    String effective_path = path_arg;
    if (path_arg == ".") {
        if (!server.GetSandboxRoots().IsEmpty()) { effective_path = server.GetSandboxRoots()[0]; }
        else { server.Log("Warning: ums-listdir for '.' with no sandbox roots, using current working directory."); effective_path = GetCurrentDirectory(); }
    }
    server.EnforceSandbox(effective_path);
    JsonArray result_array; FindFile ff(AppendFileName(effective_path, "*.*"));
    while (ff) {
        JsonObject file_entry; file_entry("name", ff.GetName());
        file_entry("is_dir", ff.IsDirectory()); file_entry("is_file", ff.IsFile());
        if (ff.IsFile()) { file_entry("size", ff.GetLength()); }
        result_array.Add(file_entry); ff.Next();
    }
    server.Log("ums-listdir success for path: " + effective_path + ". Found " + AsString(result_array.GetCount()) + " items.");
    return result_array;
}

// Renamed from SaveDataTool to WriteFileTool to match new name "ums-writefile"
Value WriteFileTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-writefile (WriteFileTool) invoked. Args: " + AsJson(args));
    if (!server.GetPermissions().allowWriteFiles) {
        throw Exc("Permission denied: Write Files permission is required for 'ums-writefile' tool.");
    }
    String path = args.Get("path", "").ToString();
    if (path.IsEmpty()) {
        throw Exc("Argument error: 'path' is a required string argument for 'ums-writefile' tool.");
    }
    if (args.Find("data") < 0) {
         throw Exc("Argument error: 'data' (string content) is a required argument for 'ums-writefile' tool.");
    }
    String data_to_save = args.Get("data", "").ToString();
    server.EnforceSandbox(path);
    if (!SaveFile(path, data_to_save)) {
        throw Exc("File system error: Failed to save data to file '" + path + "'. Check path and OS permissions.");
    }
    server.Log("ums-writefile success: Data saved to '" + path + "'.");
    return true;
}

class McpApplication {
public:
    McpApplication() : mcpServer(currentConfig.serverPort) {
        installPath = GetExeFolder(); RLOG("MCP Server starting from: " + installPath);
        cfgDir = NormalizePath(AppendFileName(installPath, "config"));
        if (!DirectoryExists(cfgDir)) RealizeDirectory(cfgDir);
        logDir = NormalizePath(AppendFileName(cfgDir, "log"));
        if (!DirectoryExists(logDir)) RealizeDirectory(logDir);
        logFilePath = NormalizePath(AppendFileName(logDir, "mcpserver.log")); RLOG("Log file path: " + logFilePath);
        cfgPath = NormalizePath(AppendFileName(cfgDir, "config.json"));
        if (!ConfigManager::Load(cfgPath, currentConfig)) {
            RLOG("Configuration missing or invalid (" + cfgPath + "); resetting to defaults.");
            currentConfig = Config(); ConfigManager::Save(cfgPath, currentConfig);
            RLOG("Default configuration saved to: " + cfgPath);
        } else { RLOG("Configuration loaded successfully from: " + cfgPath); }

        mcpServer.SetLogCallback([this](const String& msg) { ProcessServerLogMessage(msg); });
        mcpServer.SetPort(currentConfig.serverPort);
        mcpServer.ConfigureBind(currentConfig.bindAllInterfaces);
        mcpServer.Log("McpApplication initialized. McpServer log callback configured.");
        RegisterTools(); // Register tools with new names

        Ctrl::Initialize(); Ctrl::SetLanguage(LNG_ENGLISH);
        mainWindow.Create(mcpServer, currentConfig);
        mainWindow.Sizeable().Zoomable().CenterScreen(); mainWindow.Run();
    }
    ~McpApplication() { RLOG("McpApplication shutting down."); }
private:
    void RegisterTools() {
        auto Register = [&](const String& name, const String& desc, const JsonObject& params, auto func) {
            ToolDefinition def; def.description = desc; def.parameters = params;
            def.func = [this, func](const JsonObject& args) -> Value { return func(this->mcpServer, args); };
            mcpServer.AddTool(name, def); // Name used here is the key
        };

        JsonObject params; // Re-use params object for clarity for each tool

        params.Clear(); // ums-readfile
        params("path", JsonObject("type", "string")("description", "Full path to a text file."));
        Register("ums-readfile", "Read a text file’s full contents. Requires Read Files and sandbox.", params, ReadFileTool);

        params.Clear(); // ums-calc
        params("a", JsonObject("type","number")("description","First operand"))
              ("b", JsonObject("type","number")("description","Second operand"))
              ("operation", JsonObject("type","string")("description","\"add\"|\"subtract\"|\"multiply\"|\"divide\""));
        Register("ums-calc", "Perform add, subtract, multiply, divide on two numbers.", params, CalculateTool);

        params.Clear(); // ums-createdir
        params("path", JsonObject("type","string")("description","Full path for new folder."));
        Register("ums-createdir", "Create directory at specified path. Requires Create Directories and sandbox.", params, CreateDirTool);

        params.Clear(); // ums-listdir
        params("path", JsonObject("type","string")("optional", true)("description","Directory path (defaults to current)."));
        Register("ums-listdir", "List files and folders in a directory. Requires Search Directories and sandbox.", params, ListDirTool);

        params.Clear(); // ums-writefile (was save_data)
        params("path", JsonObject("type","string")("description","Full file path."))
              ("data", JsonObject("type","string")("description","Text content to write."));
        Register("ums-writefile", "Write text to file at given path. Requires Write Files and sandbox.", params, WriteFileTool); // Changed to WriteFileTool

        mcpServer.Log("All standard tools registered with new names for main server instance.");
    }
    void ProcessServerLogMessage(const String& msg) {
        String timestampedMsg = "[" + FormatIso8601(GetSysTime()) + "] [Server] " + msg;
        RLOG(timestampedMsg);
        if (mainWindow.IsOpen()) { mainWindow.AppendLog(msg); }
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
    SetExitCode(0); RLOG("Application starting...");
    McpApplication mcp_app;
    RLOG("Application main function finished. Exit code: " + AsString(GetExitCode()));
}
