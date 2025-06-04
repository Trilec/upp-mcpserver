/**
 * @file Main.cpp
 * @brief Main application entry point for the U++ MCP Server.
 */

#include <CtrlLib/CtrlLib.h>
#include "../include/McpServer.h" // Path relative to McpServerGUI/
#include <mcp_server_lib/ConfigManager.h> // For Config struct & ConfigManager class
#include "McpServerWindow.h"
#include <Core/Compress/Compress.h>
#include <Core/IO/FileStrm.h>
#include <Core/IO/FindFile.h>
#include <Core/IO/PathUtil.h>
#include <Core/TimeDate.h>
#include <Core/Trace/Trace.h>
#include <Core/ValueUtil.h>

using namespace Upp;

// Tool function implementations (remain the same)
Value ReadFileTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-readfile (ReadFileTool) invoked. Args: " + AsJson(args));
    if (!server.GetPermissions().allowReadFiles) throw Exc("Permission denied: Read Files required.");
    String path = args.Get("path", "").ToString(); if (path.IsEmpty()) throw Exc("Argument error: 'path' required.");
    server.EnforceSandbox(path); String content = LoadFile(path);
    if (content.IsVoid()) throw Exc("File error: Could not read file '" + path + "'.");
    server.Log("ums-readfile success for path: " + path); return content;
}
Value CalculateTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-calc (CalculateTool) invoked. Args: " + AsJson(args));
    Value val_a=args.Get("a"),val_b=args.Get("b"); String op=args.Get("operation","").ToString();
    if(!val_a.IsNumber())throw Exc("Argument error: 'a' must be a number for 'ums-calc' tool.");
    if(!val_b.IsNumber())throw Exc("Argument error: 'b' must be a number for 'ums-calc' tool.");
    if(op.IsEmpty())throw Exc("Argument error: 'operation' (string: add, subtract, multiply, divide) is required for 'ums-calc' tool.");
    double a=val_a.To<double>(),b=val_b.To<double>();
    if(op=="add")return a+b;if(op=="subtract")return a-b;if(op=="multiply")return a*b;
    if(op=="divide"){if(b==0)throw Exc("Arithmetic error: Division by zero in 'ums-calc' tool.");return a/b;}
    throw Exc("Argument error: Unknown operation '" + op + "'. Supported: add, subtract, multiply, divide.");
}
Value CreateDirTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-createdir (CreateDirTool) invoked. Args: " + AsJson(args));
    if(!server.GetPermissions().allowCreateDirs)throw Exc("Permission denied: Create Dirs.");
    String p=args.Get("path","").ToString(); if(p.IsEmpty())throw Exc("'path' required.");
    server.EnforceSandbox(p); if(DirectoryExists(p)){server.Log("Dir '"+p+"' already exists.");return true;}
    if(!RealizeDirectory(p))throw Exc("Failed to create dir: "+p);
    server.Log("Dir '"+p+"' created."); return true;
}
Value ListDirTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-listdir (ListDirTool) invoked. Args: " + AsJson(args));
    if(!server.GetPermissions().allowSearchDirs)throw Exc("Permission denied: Search Dirs.");
    String pa=args.Get("path",".").ToString(),ep=pa;
    if(pa=="."){if(!server.GetSandboxRoots().IsEmpty())ep=server.GetSandboxRoots()[0];
                else {server.Log("Warning: listdir '.' no sandbox, using CWD.");ep=GetCurrentDirectory();}}
    server.EnforceSandbox(ep);JsonArray ra;FindFile ff(AppendFileName(ep,"*.*"));
    while(ff){JsonObject fe;fe("name",ff.GetName())("is_dir",ff.IsDirectory())("is_file",ff.IsFile());
              if(ff.IsFile())fe("size",ff.GetLength());ra.Add(fe);ff.Next();}
    server.Log("listdir success for '"+ep+"', "+AsString(ra.GetCount())+" items.");return ra;
}
Value WriteFileTool(McpServer& server, const JsonObject& args) {
    server.Log("ums-writefile (WriteFileTool) invoked. Args: " + AsJson(args));
    if(!server.GetPermissions().allowWriteFiles)throw Exc("Permission denied: Write Files.");
    String p=args.Get("path","").ToString();if(p.IsEmpty())throw Exc("'path' required.");
    if(args.Find("data")<0)throw Exc("'data' required.");String d=args.Get("data","").ToString();
    server.EnforceSandbox(p);if(!SaveFile(p,d))throw Exc("Failed to save file: "+p);
    server.Log("Data saved to '"+p+"'.");return true;
}

class McpApplication {
public:
    // Initialize mcpServer with port from (default) currentConfig and a default path prefix
    McpApplication() : mcpServer(currentConfig.serverPort,
                                 currentConfig.ws_path_prefix.IsEmpty() ? "/mcp" : currentConfig.ws_path_prefix) {
        installPath = GetExeFolder(); RLOG("MCP Server starting from: " + installPath);
        cfgDir = NormalizePath(AppendFileName(installPath, "config"));
        if (!DirectoryExists(cfgDir)) RealizeDirectory(cfgDir);
        logDir = NormalizePath(AppendFileName(cfgDir, "log"));
        if (!DirectoryExists(logDir)) RealizeDirectory(logDir);
        logFilePath = NormalizePath(AppendFileName(logDir, "mcpserver.log")); RLOG("Log file path: " + logFilePath);
        cfgPath = NormalizePath(AppendFileName(cfgDir, "config.json"));

        if (!ConfigManager::Load(cfgPath, currentConfig)) {
            RLOG("Config missing/invalid (" + cfgPath + "); resetting to defaults.");
            currentConfig = Config(); // Creates default config
            // Ensure default path prefix and TLS settings are in Config struct if used before load
            if (currentConfig.ws_path_prefix.IsEmpty()) currentConfig.ws_path_prefix = "/mcp";
            ConfigManager::Save(cfgPath, currentConfig);
            RLOG("Default config saved: " + cfgPath);
        } else { RLOG("Config loaded: " + cfgPath); }

        mcpServer.SetLogCallback([this](const String& msg) { ProcessServerLogMessage(msg); });

        // Apply loaded/default config to server instance
        mcpServer.SetPort(currentConfig.serverPort);
        mcpServer.ConfigureBind(currentConfig.bindAllInterfaces);
        mcpServer.SetPathPrefix(currentConfig.ws_path_prefix.IsEmpty() ? "/mcp" : currentConfig.ws_path_prefix);
        mcpServer.SetTls(currentConfig.use_tls, currentConfig.tls_cert_path, currentConfig.tls_key_path);
        // Permissions and sandbox roots are applied by McpServerWindow before server starts.

        mcpServer.Log("McpApplication initialized. McpServer log callback configured.");
        RegisterTools();

        Ctrl::Initialize(); Ctrl::SetLanguage(LNG_ENGLISH);
        mainWindow.Create(mcpServer, currentConfig);

        pumpingTimer.Set(-30, THISBACK(PeriodicServerPump));

        mainWindow.Sizeable().Zoomable().CenterScreen();
        mainWindow.Run();

        pumpingTimer.Kill();
        if(mcpServer.IsListening()) mcpServer.StopServer();
    }
    ~McpApplication() { RLOG("McpApplication shutting down."); }

private:
    void PeriodicServerPump() {
        mcpServer.PumpEvents();
    }

    void RegisterTools() {
        auto Register = [&](const String& name, const String& desc, const JsonObject& params, auto func) {
            ToolDefinition def; def.description = desc; def.parameters = params;
            def.func = [this, func](const JsonObject& args) -> Value { return func(this->mcpServer, args); };
            mcpServer.AddTool(name, def);
        };
        JsonObject params;
        params.Clear(); params("path", JsonObject("type", "string")("description", "Full path to a text file."));
        Register("ums-readfile", "Read a text file’s full contents. Requires Read Files and sandbox.", params, ReadFileTool);
        params.Clear(); params("a", JsonObject("type","number")("description","First operand"))("b", JsonObject("type","number")("description","Second operand"))("operation", JsonObject("type","string")("description","\"add\"|\"subtract\"|\"multiply\"|\"divide\""));
        Register("ums-calc", "Perform add, subtract, multiply, divide on two numbers.", params, CalculateTool);
        params.Clear(); params("path", JsonObject("type","string")("description","Full path for new folder."));
        Register("ums-createdir", "Create directory at specified path. Requires Create Directories and sandbox.", params, CreateDirTool);
        params.Clear(); params("path", JsonObject("type","string")("optional", true)("description","Directory path (defaults to current)."));
        Register("ums-listdir", "List files and folders in a directory. Requires Search Directories and sandbox.", params, ListDirTool);
        params.Clear(); params("path", JsonObject("type","string")("description","Full file path."))("data", JsonObject("type","string")("description","Text content to write."));
        Register("ums-writefile", "Write text to file at given path. Requires Write Files and sandbox.", params, WriteFileTool);
        mcpServer.Log("All standard tools registered for main server instance.");
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
                RLOG("Log file renamed to: " + archiveBase); String archiveGz = archiveBase + ".gz";
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
    Config currentConfig;
    McpServer mcpServer;
    McpServerWindow mainWindow;
    Timer pumpingTimer;
};

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_FILE | LOG_TIMESTAMP | LOG_APPEND, NormalizePath(AppendFileName(GetExeFolder(), "mcpserver_startup.log")));
    SetExitCode(0); RLOG("Application starting...");
    McpApplication mcp_app;
    RLOG("Application main function finished. Exit code: " + AsString(GetExitCode()));
}
