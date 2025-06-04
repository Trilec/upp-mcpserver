#include "ConfigManager.h"
#include <Core/Core.h>
#include <Core/Json.h>
#include <Core/IO/FileUtil.h>
#include <Core/IO/PathUtil.h>
#include <Core/Trace/Trace.h>
#include <Core/ValueUtil.h>    // For GetErrorText
#include <Core/Util/Util.h> // For GetLastSystemError

#ifdef PLATFORM_POSIX
#include <sys/stat.h>  // for chmod
#endif

using namespace Upp;

// Config::Config() is now in ConfigManager.h

bool ConfigManager::Load(const String& path, Config& out) {
    if(!FileExists(path)){
        LOG("ConfigManager::Load - File not found: " + path + ". Applying default configuration.");
        out = Config();
        return true;
    }
    String js_content=LoadFile(path);
    if(js_content.IsVoid()){
        LOG("ConfigManager::Load - Failed to load content from file: " + path + ". Applying default configuration.");
        out = Config();
        return false;
    }
    if(js_content.IsEmpty()){
        LOG("ConfigManager::Load - Config file is empty: " + path + ". Applying default configuration.");
        out = Config();
        return true;
    }
    try{
        Value v=ParseJSON(js_content);
        if(v.IsError()){LOG("ConfigManager::Load - Failed to parse JSON from file '" + path + "'. Error: " + GetErrorText(v) + ". Applying defaults."); out = Config(); return false;}
        if(!v.Is<ValueMap>()){LOG("ConfigManager::Load - Parsed content from '" + path + "' is not a JSON object. Applying defaults."); out = Config(); return false;}

        ValueMap root = v.Get<ValueMap>();
        Config default_cfg;

        Value tools_v = root.Get("enabledTools", Value(ValueArray()));
        if(tools_v.Is<ValueArray>()){
            out.enabledTools.Clear(); ValueArray toolsArray = tools_v.Get<ValueArray>();
            for(int i=0;i<toolsArray.GetCount();i++){if(toolsArray[i].Is<String>())out.enabledTools.Add(toolsArray[i].ToString());}
        } else { out.enabledTools = default_cfg.enabledTools; LOG("ConfigManager::Load - 'enabledTools' not array, using defaults.");}

        Value perms_v = root.Get("permissions", Value(ValueMap()));
        if(perms_v.Is<ValueMap>()){
            ValueMap p = perms_v.Get<ValueMap>();
            out.permissions.allowReadFiles=p.Get("allowReadFiles",default_cfg.permissions.allowReadFiles).To<bool>();
            out.permissions.allowWriteFiles=p.Get("allowWriteFiles",default_cfg.permissions.allowWriteFiles).To<bool>();
            out.permissions.allowDeleteFiles=p.Get("allowDeleteFiles",default_cfg.permissions.allowDeleteFiles).To<bool>();
            out.permissions.allowRenameFiles=p.Get("allowRenameFiles",default_cfg.permissions.allowRenameFiles).To<bool>();
            out.permissions.allowCreateDirs=p.Get("allowCreateDirs",default_cfg.permissions.allowCreateDirs).To<bool>();
            out.permissions.allowSearchDirs=p.Get("allowSearchDirs",default_cfg.permissions.allowSearchDirs).To<bool>();
            out.permissions.allowExec=p.Get("allowExec",default_cfg.permissions.allowExec).To<bool>();
            out.permissions.allowNetworkAccess=p.Get("allowNetworkAccess",default_cfg.permissions.allowNetworkAccess).To<bool>();
            out.permissions.allowExternalStorage=p.Get("allowExternalStorage",default_cfg.permissions.allowExternalStorage).To<bool>();
            out.permissions.allowChangeAttributes=p.Get("allowChangeAttributes",default_cfg.permissions.allowChangeAttributes).To<bool>();
            out.permissions.allowIPC=p.Get("allowIPC",default_cfg.permissions.allowIPC).To<bool>();
        } else { out.permissions = default_cfg.permissions; LOG("ConfigManager::Load - 'permissions' not object, using defaults.");}

        Value roots_v = root.Get("sandboxRoots", Value(ValueArray()));
        if(roots_v.Is<ValueArray>()){
            out.sandboxRoots.Clear(); ValueArray rootsArray = roots_v.Get<ValueArray>();
            for(int i=0;i<rootsArray.GetCount();i++){if(rootsArray[i].Is<String>())out.sandboxRoots.Add(rootsArray[i].ToString());}
        } else { out.sandboxRoots = default_cfg.sandboxRoots; LOG("ConfigManager::Load - 'sandboxRoots' not array, using defaults.");}

        out.serverPort= AnteValue(root.Get("serverPort"), default_cfg.serverPort).To<int>();
        out.bindAllInterfaces= AnteValue(root.Get("bindAllInterfaces"), default_cfg.bindAllInterfaces).To<bool>();
        out.maxLogSizeMB= AnteValue(root.Get("maxLogSizeMB"), default_cfg.maxLogSizeMB).To<int>();
        out.ws_path_prefix= AnteValue(root.Get("ws_path_prefix"), default_cfg.ws_path_prefix).ToString();
        out.use_tls= AnteValue(root.Get("use_tls"), default_cfg.use_tls).To<bool>();
        out.tls_cert_path= AnteValue(root.Get("tls_cert_path"), default_cfg.tls_cert_path).ToString();
        out.tls_key_path= AnteValue(root.Get("tls_key_path"), default_cfg.tls_key_path).ToString();

        if(out.ws_path_prefix.IsEmpty()||!out.ws_path_prefix.StartsWith("/")){
            LOG("ConfigManager::Load - ws_path_prefix '"+out.ws_path_prefix+"' invalid, reset to default.");
            out.ws_path_prefix=default_cfg.ws_path_prefix;
        }
        LOG("ConfigManager::Load - Success from: "+path); return true;
    }catch(const ValueTypeError&e){LOG("ConfigManager::Load - JSON type err '"+path+"': "+e.ToString()+". Applying defaults."); out = Config(); return false;}
    catch(const Exc&e){LOG("ConfigManager::Load - Exc load conf '"+path+"': "+e.ToString()+". Applying defaults."); out = Config(); return false;}
    catch(...){LOG("ConfigManager::Load - Unknown exc load conf: "+path+". Applying defaults."); out = Config(); return false;}
}

void ConfigManager::Save(const String& path, const Config& cfg) {
    ValueMap root_map;
    ValueArray tools_va; for(const auto&t:cfg.enabledTools)tools_va.Add(t); root_map.Add("enabledTools",Value(tools_va));
    ValueMap perms_map;
    perms_map.Add("allowReadFiles",cfg.permissions.allowReadFiles).Add("allowWriteFiles",cfg.permissions.allowWriteFiles)
             .Add("allowDeleteFiles",cfg.permissions.allowDeleteFiles).Add("allowRenameFiles",cfg.permissions.allowRenameFiles)
             .Add("allowCreateDirs",cfg.permissions.allowCreateDirs).Add("allowSearchDirs",cfg.permissions.allowSearchDirs)
             .Add("allowExec",cfg.permissions.allowExec).Add("allowNetworkAccess",cfg.permissions.allowNetworkAccess)
             .Add("allowExternalStorage",cfg.permissions.allowExternalStorage).Add("allowChangeAttributes",cfg.permissions.allowChangeAttributes)
             .Add("allowIPC",cfg.permissions.allowIPC);
    root_map.Add("permissions", Value(perms_map));
    ValueArray roots_va; for(const auto&r:cfg.sandboxRoots)roots_va.Add(r); root_map.Add("sandboxRoots",Value(roots_va));
    root_map.Add("serverPort",cfg.serverPort).Add("bindAllInterfaces",cfg.bindAllInterfaces).Add("maxLogSizeMB",cfg.maxLogSizeMB)
            .Add("ws_path_prefix",cfg.ws_path_prefix).Add("use_tls",cfg.use_tls)
            .Add("tls_cert_path",cfg.tls_cert_path).Add("tls_key_path",cfg.tls_key_path);
    String json_output=StoreAsJson(Value(root_map),true);
    String dir=GetFileFolder(path); if(!DirectoryExists(dir)){if(!RealizeDirectory(dir)){LOG("ConfigManager::Save - CRIT: Failed create dir: "+dir);return;}}
    if(!SaveFile(path,json_output)){LOG("ConfigManager::Save - CRIT: Failed save file: "+path);return;}
    LOG("ConfigManager::Save - Saved to: "+path);
    #ifdef PLATFORM_POSIX
    if(chmod(path,0600)!=0){LOG("ConfigManager::Save - Warn: chmod 0600 failed: "+path+". Err: "+GetLastSystemError());}
    else{LOG("ConfigManager::Save - Set perms 0600: "+path);}
    #endif
}
