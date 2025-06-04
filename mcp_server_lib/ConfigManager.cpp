#include "ConfigManager.h"
#include <Core/Json.h>    // For ParseJSON, StoreAsJson, JsonObject, JsonArray, Value
#include <Core/ValueUtil.h> // For GetErrorText
#include <Core/IO/FileStrm.h> // For FileOut (Save - though SaveFile is simpler)
#include <Core/IO/PathUtil.h> // For GetFileFolder, RealizeDirectory
#include <Core/IO/FileUtil.h> // For FileExists, LoadFile, SaveFile
#include <Core/Trace/Trace.h> // For LOG
#include <Core/Util/Util.h>   // For GetLastErrorMessage

#ifdef PLATFORM_POSIX
#include <sys/stat.h>  // for chmod on POSIX
#endif

/**
 * @file ConfigManager.cpp
 * @brief Implementation for loading and saving MCP server config (JSON).
 */

bool ConfigManager::Load(const String& path, Config& out) {
    if (!FileExists(path)) {
        LOG("ConfigManager::Load - File not found: " + path);
        return false;
    }

    String js_content = LoadFile(path);
    if (js_content.IsVoid()) {
        LOG("ConfigManager::Load - Failed to load content from file: " + path + ". Possible OS error or empty file treated as error.");
        return false;
    }

    try {
        Value v = ParseJSON(js_content);
        if (v.IsError() || !v.Is<JsonObject>()) {
            LOG("ConfigManager::Load - Failed to parse JSON from file '" + path + "' or content is not a JSON object.");
            if(v.IsError()) LOG("Parse Error: " + GetErrorText(v));
            return false;
        }
        JsonObject root = v.To<JsonObject>();

        // enabledTools
        if (root.Has("enabledTools") && root["enabledTools"].Is<JsonArray>()) {
            out.enabledTools.Clear();
            JsonArray toolsArray = root["enabledTools"].To<JsonArray>();
            for (int i = 0; i < toolsArray.GetCount(); ++i) {
                Value item = toolsArray[i];
                if (item.Is<String>()) {
                    out.enabledTools.Add(item.To<String>());
                } else { LOG("ConfigManager::Load - Warning: Non-string item in 'enabledTools' array. Skipping."); }
            }
        } else { out.enabledTools.Clear(); LOG("ConfigManager::Load - 'enabledTools' array missing or invalid, defaulting to empty."); }


        // permissions
        if (root.Has("permissions") && root["permissions"].Is<JsonObject>()) {
            JsonObject p = root["permissions"].To<JsonObject>();
            out.permissions.allowReadFiles        = p.Get("allowReadFiles", false);
            out.permissions.allowWriteFiles       = p.Get("allowWriteFiles", false);
            out.permissions.allowDeleteFiles      = p.Get("allowDeleteFiles", false);
            out.permissions.allowRenameFiles      = p.Get("allowRenameFiles", false);
            out.permissions.allowCreateDirs       = p.Get("allowCreateDirs", false);
            out.permissions.allowSearchDirs       = p.Get("allowSearchDirs", false);
            out.permissions.allowExec             = p.Get("allowExec", false);
            out.permissions.allowNetworkAccess    = p.Get("allowNetworkAccess", false);
            out.permissions.allowExternalStorage  = p.Get("allowExternalStorage", false);
            out.permissions.allowChangeAttributes = p.Get("allowChangeAttributes", false);
            out.permissions.allowIPC              = p.Get("allowIPC", false);
        } else { out.permissions = Permissions(); LOG("ConfigManager::Load - 'permissions' object missing or invalid, defaulting all to false.");}


        // sandboxRoots
        if (root.Has("sandboxRoots") && root["sandboxRoots"].Is<JsonArray>()) {
            out.sandboxRoots.Clear();
            JsonArray rootsArray = root["sandboxRoots"].To<JsonArray>();
            for (int i = 0; i < rootsArray.GetCount(); ++i) {
                Value item = rootsArray[i];
                if (item.Is<String>()) {
                    out.sandboxRoots.Add(item.To<String>());
                } else { LOG("ConfigManager::Load - Warning: Non-string item in 'sandboxRoots' array. Skipping.");}
            }
        } else { out.sandboxRoots.Clear(); LOG("ConfigManager::Load - 'sandboxRoots' array missing or invalid, defaulting to empty.");}

        Config default_cfg; // To get default values for new fields
        out.serverPort          = root.Get("serverPort", default_cfg.serverPort);
        out.bindAllInterfaces   = root.Get("bindAllInterfaces", default_cfg.bindAllInterfaces);
        out.maxLogSizeMB        = root.Get("maxLogSizeMB", default_cfg.maxLogSizeMB);
        out.ws_path_prefix      = root.Get("ws_path_prefix", default_cfg.ws_path_prefix).ToString();
        out.use_tls             = root.Get("use_tls", default_cfg.use_tls);
        out.tls_cert_path       = root.Get("tls_cert_path", default_cfg.tls_cert_path).ToString();
        out.tls_key_path        = root.Get("tls_key_path", default_cfg.tls_key_path).ToString();

        if (out.ws_path_prefix.IsEmpty() || !out.ws_path_prefix.StartsWith("/")) {
            out.ws_path_prefix = default_cfg.ws_path_prefix;
            LOG("ConfigManager::Load - Warning: ws_path_prefix was empty or invalid, reset to default: " + out.ws_path_prefix);
        }

        LOG("ConfigManager::Load - Configuration loaded successfully from: " + path);
        return true;
    }
    catch(const ValueTypeError& e) {
        LOG("ConfigManager::Load - JSON structure error in '" + path + "': " + e.ToString());
        return false;
    }
    catch(const Exc& e) {
        LOG("ConfigManager::Load - Exception while loading config '" + path + "': " + e.ToString());
        return false;
    }
    catch(...) {
        LOG("ConfigManager::Load - Unknown exception while loading config: " + path);
        return false;
    }
}

void ConfigManager::Save(const String& path, const Config& cfg) {
    JsonObject root;

    JsonArray arrTools;
    for (const auto& t : cfg.enabledTools) arrTools.Add(t);
    root("enabledTools", arrTools);

    JsonObject p;
    p("allowReadFiles",         cfg.permissions.allowReadFiles);
    p("allowWriteFiles",        cfg.permissions.allowWriteFiles);
    p("allowDeleteFiles",       cfg.permissions.allowDeleteFiles);
    p("allowRenameFiles",       cfg.permissions.allowRenameFiles);
    p("allowCreateDirs",        cfg.permissions.allowCreateDirs);
    p("allowSearchDirs",        cfg.permissions.allowSearchDirs);
    p("allowExec",              cfg.permissions.allowExec);
    p("allowNetworkAccess",     cfg.permissions.allowNetworkAccess);
    p("allowExternalStorage",   cfg.permissions.allowExternalStorage);
    p("allowChangeAttributes",  cfg.permissions.allowChangeAttributes);
    p("allowIPC",               cfg.permissions.allowIPC);
    root("permissions", p);

    JsonArray arrRoots;
    for (const auto& r : cfg.sandboxRoots) arrRoots.Add(r);
    root("sandboxRoots", arrRoots);

    root("serverPort",        cfg.serverPort);
    root("bindAllInterfaces", cfg.bindAllInterfaces);
    root("maxLogSizeMB",      cfg.maxLogSizeMB);
    root("ws_path_prefix",    cfg.ws_path_prefix);
    root("use_tls",           cfg.use_tls);
    root("tls_cert_path",     cfg.tls_cert_path);
    root("tls_key_path",      cfg.tls_key_path);

    String json_output = StoreAsJson(root, true);

    String dir = GetFileFolder(path);
    if (!DirectoryExists(dir)) {
        if(!RealizeDirectory(dir)) {
            LOG("ConfigManager::Save - CRITICAL: Failed to create config directory: " + dir);
            return;
        }
    }

    if (!SaveFile(path, json_output)) {
        LOG("ConfigManager::Save - CRITICAL: Failed to save configuration to file: " + path);
        return;
    }

    LOG("ConfigManager::Save - Configuration saved to: " + path);

    #ifdef PLATFORM_POSIX
    if (chmod(path, ST_MODE_RWXU) != 0) { // ST_MODE_RWXU is 0600 typically from U++ <sys/stat.h> wrapper or OS
        LOG("ConfigManager::Save - Warning: Failed to set permissions (0600) on: " + path + ". Error: " + GetLastSystemError());
    } else {
        LOG("ConfigManager::Save - Set permissions to 0600 on: " + path);
    }
    #endif
}
