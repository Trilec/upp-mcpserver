#include <CtrlLib/CtrlLib.h>
#include "../include/McpServer.h"
#include <mcp_server_lib/ConfigManager.h>
#include "McpServerWindow.h"
#include <Core/Compress/Compress.h>
#include <Core/IO/FileStrm.h>
#include <Core/IO/FindFile.h>
#include <Core/IO/PathUtil.h>
#include <Core/TimeDate.h>
#include <Core/Trace/Trace.h>
#include <Core/ValueUtil.h>

using namespace Upp;

// Tool functions now take const Value& args, expected to be ValueMap
Value ReadFileTool(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>()) throw Exc("ums-readfile: 'args' must be a JSON object.");
    ValueMap args = args_v.Get<ValueMap>();
    server.Log("ums-readfile invoked. Args: " + StoreAsJson(args_v, true));
    if(!server.GetPermissions().allowReadFiles) throw Exc("Perm denied: Read Files for 'ums-readfile'.");
    String path = args.Get("path", "").ToString(); if(path.IsEmpty()) throw Exc("Arg err: 'path' required for 'ums-readfile'.");
    server.EnforceSandbox(path); String content = LoadFile(path);
    if(content.IsVoid()) throw Exc("File err: Could not read file '"+path+"'.");
    server.Log("ums-readfile success: "+path); return content;
}
Value CalculateTool(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>()) throw Exc("ums-calc: 'args' must be a JSON object.");
    ValueMap args = args_v.Get<ValueMap>();
    server.Log("ums-calc invoked. Args: " + StoreAsJson(args_v, true));
    Value va=args.Get("a"),vb=args.Get("b"); String op=args.Get("operation","").ToString();
    if(!va.IsNumber())throw Exc("Arg err: 'a' num for 'ums-calc'."); if(!vb.IsNumber())throw Exc("Arg err: 'b' num for 'ums-calc'.");
    if(op.IsEmpty())throw Exc("Arg err: 'operation' for 'ums-calc'."); double a=va.To<double>(),b=vb.To<double>();
    if(op=="add")return a+b; if(op=="subtract")return a-b; if(op=="multiply")return a*b;
    if(op=="divide"){if(b==0)throw Exc("Arith err: Div by zero 'ums-calc'.");return a/b;}
    throw Exc("Arg err: Unknown op '"+op+"' for 'ums-calc'.");
}
Value CreateDirTool(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>()) throw Exc("ums-createdir: 'args' must be a JSON object.");
    ValueMap args = args_v.Get<ValueMap>();
    server.Log("ums-createdir invoked. Args: " + StoreAsJson(args_v, true));
    if(!server.GetPermissions().allowCreateDirs)throw Exc("Perm denied: Create Dirs for 'ums-createdir'.");
    String p=args.Get("path","").ToString(); if(p.IsEmpty())throw Exc("Arg err: 'path' for 'ums-createdir'.");
    server.EnforceSandbox(p); if(DirectoryExists(p)){server.Log("Dir '"+p+"' exists.");return true;}
    if(!RealizeDirectory(p))throw Exc("FS err: Failed create dir '"+p+"'.");
    server.Log("Dir '"+p+"' created."); return true;
}
Value ListDirTool(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>() && !args_v.IsVoid()) throw Exc("ums-listdir: 'args' must be a JSON object or null/void."); // Allow empty call for default path
    ValueMap args = args_v.Is<ValueMap>() ? args_v.Get<ValueMap>() : ValueMap(); // Handle void args_v for default path
    server.Log("ums-listdir invoked. Args: " + StoreAsJson(args_v, true));
    if(!server.GetPermissions().allowSearchDirs)throw Exc("Perm denied: Search Dirs for 'ums-listdir'.");
    String pa=args.Get("path",".").ToString(),ep=pa;
    if(pa=="."){if(!server.GetSandboxRoots().IsEmpty())ep=server.GetSandboxRoots()[0]; else {server.Log("Warn: listdir '.' no sandbox, CWD.");ep=GetCurrentDirectory();}}
    server.EnforceSandbox(ep);ValueArray ra;FindFile ff(AppendFileName(ep,"*.*"));
    while(ff){ValueMap fe;fe.Add("name",ff.GetName()).Add("is_dir",ff.IsDirectory()).Add("is_file",ff.IsFile());
              if(ff.IsFile())fe.Add("size",ff.GetLength());ra.Add(Value(fe));ff.Next();}
    server.Log("listdir success '"+ep+"', "+AsString(ra.GetCount())+" items.");return Value(ra);
}
Value WriteFileTool(McpServer& server, const Value& args_v) {
    if(!args_v.Is<ValueMap>()) throw Exc("ums-writefile: 'args' must be a JSON object.");
    ValueMap args = args_v.Get<ValueMap>();
    server.Log("ums-writefile invoked. Args: " + StoreAsJson(args_v, true));
    if(!server.GetPermissions().allowWriteFiles)throw Exc("Perm denied: Write Files for 'ums-writefile'.");
    String p=args.Get("path","").ToString();if(p.IsEmpty())throw Exc("Arg err: 'path' for 'ums-writefile'.");
    if(args.Find("data")<0)throw Exc("Arg err: 'data' for 'ums-writefile'.");String d=args.Get("data","").ToString();
    server.EnforceSandbox(p);if(!SaveFile(p,d))throw Exc("FS err: Failed save '"+p+"'.");
    server.Log("Data saved '"+p+"'.");return true;
}

class McpApplication{
public:
    McpApplication():mcpServer(currentConfig.serverPort, currentConfig.ws_path_prefix.IsEmpty() ? "/mcp" : currentConfig.ws_path_prefix){
        installPath=GetExeFolder();RLOG("MCP Server starting from: "+installPath);cfgDir=NormalizePath(AppendFileName(installPath,"config"));
        if(!DirectoryExists(cfgDir))RealizeDirectory(cfgDir);logDir=NormalizePath(AppendFileName(cfgDir,"log"));
        if(!DirectoryExists(logDir))RealizeDirectory(logDir);logFilePath=NormalizePath(AppendFileName(logDir,"mcpserver.log"));RLOG("Log file path: "+logFilePath);
        cfgPath=NormalizePath(AppendFileName(cfgDir,"config.json"));
        if(!ConfigManager::Load(cfgPath,currentConfig)){RLOG("Conf missing/invalid ("+cfgPath+"); defaults.");currentConfig=Config();ConfigManager::Save(cfgPath,currentConfig);RLOG("Default conf saved: "+cfgPath);}
        else{RLOG("Conf loaded: "+cfgPath);}
        mcpServer.SetLogCallback([this](const String&msg){ProcessServerLogMessage(msg);});
        mcpServer.SetPort(currentConfig.serverPort);mcpServer.ConfigureBind(currentConfig.bindAllInterfaces);
        mcpServer.SetPathPrefix(currentConfig.ws_path_prefix.IsEmpty()?"/mcp":currentConfig.ws_path_prefix);
        mcpServer.SetTls(currentConfig.use_tls,currentConfig.tls_cert_path,currentConfig.tls_key_path);
        mcpServer.Log("McpApp init. Log cb conf.");
        RegisterTools();
        Ctrl::Initialize();Ctrl::SetLanguage(LNG_ENGLISH);
        mainWindow.Create(mcpServer,currentConfig);pumpingTimer.Set(-30,THISBACK(PeriodicServerPump));
        mainWindow.Sizeable().Zoomable().CenterScreen();mainWindow.Run();pumpingTimer.Kill();
        if(mcpServer.IsListening())mcpServer.StopServer();
    }
    ~McpApplication(){RLOG("McpApp shutting down.");}
private:
    void PeriodicServerPump(){mcpServer.PumpEvents();}
    void RegisterTools(){
        auto Register=[this](const String&name,const String&desc,const Value&params_v,auto func){
            ToolDefinition def;def.description=desc;def.parameters=params_v;
            def.func=[this,func](const Value&args_v)->Value{return func(this->mcpServer,args_v);};
            mcpServer.AddTool(name,def);
        };
        ValueMap params_map;
        params_map.Clear();params_map.Add("path",Value(ValueMap("type","string")("description","Full path to text file.")));
        Register("ums-readfile","Reads file. Needs Read Files & sandbox.",Value(params_map),ReadFileTool);
        params_map.Clear();params_map.Add("a",ValueMap("type","number")("description","First op")) .Add("b",ValueMap("type","number")("description","Second op")) .Add("operation",ValueMap("type","string")("description","add|sub|mul|div"));
        Register("ums-calc","Basic arithmetic.",Value(params_map),CalculateTool);
        params_map.Clear();params_map.Add("path",Value(ValueMap("type","string")("description","New folder path.")));
        Register("ums-createdir","Creates dir. Needs Create Dirs & sandbox.",Value(params_map),CreateDirTool);
        params_map.Clear();params_map.Add("path",Value(ValueMap("type","string")("optional",true)("description","Dir path (default .).")));
        Register("ums-listdir","Lists dir. Needs Search Dirs & sandbox.",Value(params_map),ListDirTool);
        params_map.Clear();params_map.Add("path",Value(ValueMap("type","string")("description","File path"))).Add("data",Value(ValueMap("type","string")("description","Text content")));
        Register("ums-writefile","Writes text to file. Needs Write Files & sandbox.",Value(params_map),WriteFileTool);
        mcpServer.Log("Std tools registered with Value-based params for main server.");
    }
    void ProcessServerLogMessage(const String&msg){
        String tsMsg="["+FormatIso8601(GetSysTime())+"] [S] "+msg;RLOG(tsMsg);
        if(mainWindow.IsOpen())mainWindow.AppendLog(msg);FileAppend logFile(logFilePath);
        if(logFile.IsOpen()){logFile.PutLine(tsMsg);logFile.Close();}else{StdLog().Put("CRIT log fail: "+logFilePath+" Msg: "+tsMsg+"");} // Removed newline from log
        int64 sz=GetFileLength(logFilePath);if(sz>(int64)currentConfig.maxLogSizeMB*1024*1024&&sz>0){
        String tt=FormatTime(GetSysTime(),"YYYYMMDD_HHMMSS");String ab=NormalizePath(AppendFileName(logDir,"mcpserver_"+tt+".log"));
        RLOG("Log rotate. Sz:"+AsString(sz)+"B. Max:"+AsString(currentConfig.maxLogSizeMB)+"MB.");
        if(RenameFile(logFilePath,ab)){RLOG("Log renamed: "+ab);String ag=ab+".gz";
            if(GZCompressFile(ab,ag)){RLOG("Log compressed: "+ag);DeleteFile(ab);if(mainWindow.IsOpen())mainWindow.AppendLog("Log rotated: "+ag);}
            else{RLOG("Fail compress log: "+ab);if(mainWindow.IsOpen())mainWindow.AppendLog("Fail compress rotated: "+ab);}}
        else{RLOG("Fail rename log for rotate: "+logFilePath);if(mainWindow.IsOpen())mainWindow.AppendLog("Fail rename log for rotate.");}
        FileOut nl(logFilePath);if(nl.IsOpen()){nl.PutLine("["+FormatIso8601(GetSysTime())+"] [S] Log rotated. Prev log archived ("+AsString(sz>>20)+"MB).");nl.Close();}}}
    String installPath,cfgDir,logDir,cfgPath,logFilePath;Config currentConfig;McpServer mcpServer;McpServerWindow mainWindow;Timer pumpingTimer;
};
CONSOLE_APP_MAIN{StdLogSetup(LOG_FILE|LOG_TIMESTAMP|LOG_APPEND,NormalizePath(AppendFileName(GetExeFolder(),"mcpserver_startup.log")));SetExitCode(0);RLOG("App starting...");McpApplication mcp_app;RLOG("App main finished. Exit: "+AsString(GetExitCode()));}
