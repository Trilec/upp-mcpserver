#include "ConfigManager.h"               // Corresponding header first
#include <Core/Core.h>                  // For basic types if not in ConfigManager.h
#include <Core/Json.h>                  // For ParseJSON, StoreAsJson
#include <Core/IO/FileUtil.h>           // For FileExists, LoadFile, SaveFile
#include <Core/IO/PathUtil.h>           // For GetFileFolder, RealizeDirectory
#include <Core/Trace/Trace.h>           // For LOG
#include <Core/ValueUtil.h>             // For GetErrorText (from Value)
#include <Core/Util/Util.h>             // For GetLastSystemError

#ifdef PLATFORM_POSIX
#include <sys/stat.h>  // for chmod
#endif

using namespace Upp; // Added

/**
 * @file ConfigManager.cpp
 * @brief Implementation for loading and saving MCP server config (JSON).
 */

// Default constructor for Config (already in .h as of last step, ensure consistency)
// Config::Config() : serverPort(5000), bindAllInterfaces(false), maxLogSizeMB(10), ws_path_prefix("/mcp"), use_tls(false) {
// } // This is defined in the header now.

bool ConfigManager::Load(const String& path, Config& out) {
    if (!FileExists(path)) {
        LOG("ConfigManager::Load - File not found: " + path + ". Applying default configuration.");
        out = Config(); // Apply default configuration
        // Optionally save the default config here if that's desired behavior
        // ConfigManager::Save(path, out);
        return true; // Return true because defaults are applied, not a fatal error for the app.
                     // Or return false if a config file MUST exist. Current design implies defaults are OK.
    }

    String js_content = LoadFile(path);
    if (js_content.IsVoid()) { // IsVoid means error during LoadFile
        LOG("ConfigManager::Load - Failed to load content from file: " + path + ". Possible OS error. Applying default configuration.");
        out = Config();
        return false; // Indicate that loading from file failed, even if defaults are applied.
                      // This signals to the caller that the persisted state was not used.
    }

    if (js_content.IsEmpty()) {
        LOG("ConfigManager::Load - Config file is empty: " + path + ". Applying default configuration.");
        out = Config();
        return true; // Empty file might mean "use defaults".
    }

    try {
        Value v = ParseJSON(js_content);
        if (v.IsError()) {
            LOG("ConfigManager::Load - Failed to parse JSON from file '" + path + "'. Error: " + GetErrorText(v) + ". Applying defaults.");
            out = Config();
            return false; // Parsing error means persisted state is corrupt/unusable.
        }
        if (!v.Is<JsonObject>()) {
             LOG("ConfigManager::Load - Parsed content from '" + path + "' is not a JSON object. Applying defaults.");
            out = Config();
            return false; // Invalid format.
        }
        JsonObject root = v.To<JsonObject>();
        Config default_cfg; // For sourcing default values for individual fields

        // enabledTools
        if (root.Has("enabledTools") && root["enabledTools"].Is<JsonArray>()) {
            out.enabledTools.Clear();
            JsonArray toolsArray = root["enabledTools"].To<JsonArray>();
            for (int i = 0; i < toolsArray.GetCount(); ++i) {
                if (toolsArray[i].Is<String>()) out.enabledTools.Add(toolsArray[i].To<String>());
                else LOG("ConfigManager::Load - Warning: Non-string item in 'enabledTools'. Skipping.");
            }
        } else { out.enabledTools = default_cfg.enabledTools; LOG("ConfigManager::Load - 'enabledTools' missing/invalid, using defaults."); }

        // permissions
        if (root.Has("permissions") && root["permissions"].Is<JsonObject>()) {
            JsonObject p = root["permissions"].To<JsonObject>();
            out.permissions.allowReadFiles = p.Get("allowReadFiles", default_cfg.permissions.allowReadFiles);
            out.permissions.allowWriteFiles       = p.Get("allowWriteFiles", default_cfg.permissions.allowWriteFiles);
            out.permissions.allowDeleteFiles      = p.Get("allowDeleteFiles", default_cfg.permissions.allowDeleteFiles);
            out.permissions.allowRenameFiles      = p.Get("allowRenameFiles", default_cfg.permissions.allowRenameFiles);
            out.permissions.allowCreateDirs       = p.Get("allowCreateDirs", default_cfg.permissions.allowCreateDirs);
            out.permissions.allowSearchDirs       = p.Get("allowSearchDirs", default_cfg.permissions.allowSearchDirs);
            out.permissions.allowExec             = p.Get("allowExec", default_cfg.permissions.allowExec);
            out.permissions.allowNetworkAccess    = p.Get("allowNetworkAccess", default_cfg.permissions.allowNetworkAccess);
            out.permissions.allowExternalStorage  = p.Get("allowExternalStorage", default_cfg.permissions.allowExternalStorage);
            out.permissions.allowChangeAttributes = p.Get("allowChangeAttributes", default_cfg.permissions.allowChangeAttributes);
            out.permissions.allowIPC              = p.Get("allowIPC", default_cfg.permissions.allowIPC);
        } else { out.permissions = default_cfg.permissions; LOG("ConfigManager::Load - 'permissions' missing/invalid, using defaults.");}

        // sandboxRoots
        if (root.Has("sandboxRoots") && root["sandboxRoots"].Is<JsonArray>()) {
            out.sandboxRoots.Clear();
            JsonArray rootsArray = root["sandboxRoots"].To<JsonArray>();
            for (int i = 0; i < rootsArray.GetCount(); ++i) {
                if (rootsArray[i].Is<String>()) out.sandboxRoots.Add(rootsArray[i].To<String>());
                else LOG("ConfigManager::Load - Warning: Non-string item in 'sandboxRoots'. Skipping.");
            }
        } else { out.sandboxRoots = default_cfg.sandboxRoots; LOG("ConfigManager::Load - 'sandboxRoots' missing/invalid, using defaults.");}

        out.serverPort          = root.Get("serverPort", default_cfg.serverPort);
        out.bindAllInterfaces   = root.Get("bindAllInterfaces", default_cfg.bindAllInterfaces);
        out.maxLogSizeMB        = root.Get("maxLogSizeMB", default_cfg.maxLogSizeMB);
        out.ws_path_prefix      = root.Get("ws_path_prefix", default_cfg.ws_path_prefix).ToString();
        out.use_tls             = root.Get("use_tls", default_cfg.use_tls);
        out.tls_cert_path       = root.Get("tls_cert_path", default_cfg.tls_cert_path).ToString();
        out.tls_key_path        = root.Get("tls_key_path", default_cfg.tls_key_path).ToString();

        if (out.ws_path_prefix.IsEmpty() || !out.ws_path_prefix.StartsWith("/")) {
            LOG("ConfigManager::Load - Warning: ws_path_prefix ('" + out.ws_path_prefix + "') was invalid, reset to default: " + default_cfg.ws_path_prefix);
            out.ws_path_prefix = default_cfg.ws_path_prefix;
        }

        LOG("ConfigManager::Load - Configuration loaded successfully from: " + path);
        return true;
    }
    catch(const ValueTypeError& e) { LOG("ConfigManager::Load - JSON structure error in '" + path + "': " + e.ToString() + ". Applying defaults."); out = Config(); return false;}
    catch(const Exc& e) { LOG("ConfigManager::Load - Exception loading config '" + path + "': " + e.ToString() + ". Applying defaults."); out = Config(); return false;}
    catch(...) { LOG("ConfigManager::Load - Unknown exception loading config: " + path + ". Applying defaults."); out = Config(); return false;}
}

void ConfigManager::Save(const String& path, const Config& cfg) {
    JsonObject root;
    JsonArray arrTools; for(const auto& t : cfg.enabledTools) arrTools.Add(t); root("enabledTools", arrTools);
    JsonObject p;
    p("allowReadFiles",cfg.permissions.allowReadFiles)("allowWriteFiles",cfg.permissions.allowWriteFiles)
     ("allowDeleteFiles",cfg.permissions.allowDeleteFiles)("allowRenameFiles",cfg.permissions.allowRenameFiles)
     ("allowCreateDirs",cfg.permissions.allowCreateDirs)("allowSearchDirs",cfg.permissions.allowSearchDirs)
     ("allowExec",cfg.permissions.allowExec)("allowNetworkAccess",cfg.permissions.allowNetworkAccess)
     ("allowExternalStorage",cfg.permissions.allowExternalStorage)("allowChangeAttributes",cfg.permissions.allowChangeAttributes)
     ("allowIPC",cfg.permissions.allowIPC);
    root("permissions", p);
    JsonArray arrRoots; for(const auto& r : cfg.sandboxRoots) arrRoots.Add(r); root("sandboxRoots", arrRoots);
    root("serverPort",cfg.serverPort)("bindAllInterfaces",cfg.bindAllInterfaces)("maxLogSizeMB",cfg.maxLogSizeMB)
        ("ws_path_prefix",cfg.ws_path_prefix)("use_tls",cfg.use_tls)
        ("tls_cert_path",cfg.tls_cert_path)("tls_key_path",cfg.tls_key_path);
    String json_output = StoreAsJson(root, true);
    String dir = GetFileFolder(path);
    if (!DirectoryExists(dir)) { if(!RealizeDirectory(dir)) { LOG("ConfigManager::Save - CRITICAL: Failed to create config directory: " + dir); return; }}
    if (!SaveFile(path, json_output)) { LOG("ConfigManager::Save - CRITICAL: Failed to save to file: " + path); return; }
    LOG("ConfigManager::Save - Configuration saved to: " + path);
    #ifdef PLATFORM_POSIX
    if(chmod(path, ST_MODE_RWXU)!=0){LOG("ConfigManager::Save - Warning: chmod 0600 failed for: "+path+". Error: "+GetLastSystemError());}
    else{LOG("ConfigManager::Save - Set permissions to 0600 on: " + path);}
    #endif
}
