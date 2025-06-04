#include "ConfigManager.h" // Defines Config struct and ConfigManager class
#include <Core/Json.h>    // For Json parsing (ParseJSON, StoreAsJson)
#include <sys/stat.h>  // for chmod on POSIX, as per pseudocode

// Note: <Core/Core.h> provides FileExists, FileIn, FileOut, LOG, GetFileFolder, DirectoryExists, RealizeDirectory
// It's assumed to be included via ConfigManager.h -> McpServer.h -> Core/Core.h

/**
 * @file ConfigManager.cpp
 * @brief Implementation for loading and saving MCP server config (JSON).
 */

bool ConfigManager::Load(const String& path, Config& out) {
    if (!FileExists(path)) {
        LOG("ConfigManager::Load - File not found: " + path);
        return false;
    }
    try {
        FileIn in(path);
        if (!in.IsOpen()) {
            LOG("ConfigManager::Load - Could not open file: " + path);
            return false;
        }
        String js = in.GetText(CHARSET_UTF8); // Read entire file as UTF-8 text
        in.Close();

        Value v = ParseJSON(js); // U++ uses ParseJSON
        if (v.IsError() || !v.Is<JsonObject>()) {
            LOG("ConfigManager::Load - Failed to parse JSON or not an object: " + path);
            if(v.IsError()) LOG("Parse Error: " + v.ToString());
            return false;
        }
        JsonObject root = v.To<JsonObject>();

        // enabledTools
        if (!root.Has("enabledTools") || !root["enabledTools"].Is<JsonArray>()) {
            LOG("ConfigManager::Load - Missing or invalid 'enabledTools' array.");
            return false;
        }
        out.enabledTools.Clear();
        JsonArray toolsArray = root["enabledTools"].To<JsonArray>();
        for (int i = 0; i < toolsArray.GetCount(); ++i) {
            Value item = toolsArray[i];
            if (!item.Is<String>()) {
                LOG("ConfigManager::Load - Non-string item in 'enabledTools' array.");
                return false;
            }
            out.enabledTools.Add(item.To<String>());
        }

        // permissions
        if (!root.Has("permissions") || !root["permissions"].Is<JsonObject>()) {
            LOG("ConfigManager::Load - Missing or invalid 'permissions' object.");
            return false;
        }
        JsonObject p = root["permissions"].To<JsonObject>();
        out.permissions.allowReadFiles        = p.Get("allowReadFiles", false); // Safe get with default
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

        // sandboxRoots
        if (!root.Has("sandboxRoots") || !root["sandboxRoots"].Is<JsonArray>()) {
            LOG("ConfigManager::Load - Missing or invalid 'sandboxRoots' array.");
            return false;
        }
        out.sandboxRoots.Clear();
        JsonArray rootsArray = root["sandboxRoots"].To<JsonArray>();
        for (int i = 0; i < rootsArray.GetCount(); ++i) {
            Value item = rootsArray[i];
            if (!item.Is<String>()) {
                LOG("ConfigManager::Load - Non-string item in 'sandboxRoots' array.");
                return false;
            }
            out.sandboxRoots.Add(item.To<String>());
        }

        // serverPort
        out.serverPort = root.Get("serverPort", 5000); // Safe get
        // bindAllInterfaces
        out.bindAllInterfaces = root.Get("bindAllInterfaces", false); // Safe get
        // maxLogSizeMB
        out.maxLogSizeMB = root.Get("maxLogSizeMB", 10); // Safe get

        LOG("ConfigManager::Load - Configuration loaded successfully from: " + path);
        return true;
    }
    catch(const ValueTypeError& e) { // Specific U++ Value type error
        LOG("ConfigManager::Load - JSON structure error in '" + path + "': " + e);
        return false;
    }
    catch(const Exc& e) { // General U++ exception
        LOG("ConfigManager::Load - Exception while loading config '" + path + "': " + e);
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

    String json = StoreAsJson(root, true); // true for pretty printing

    // Ensure directory exists before writing
    String dir = GetFileFolder(path);
    if (!DirectoryExists(dir)) {
        RealizeDirectory(dir); // U++ function to create directory recursively
    }

    FileOut fout(path);
    if (!fout.IsOpen()) {
        LOG("ConfigManager::Save - Failed to open file for writing: " + path);
        // Optionally throw an exception here or return bool
        return;
    }
    fout.Put(json); // Write string to file
    fout.Close();

    LOG("ConfigManager::Save - Configuration saved to: " + path);

    // Make config.json owner-only on POSIX
    #ifdef PLATFORM_POSIX
    if (chmod(path, 0600) != 0) {
        LOG("ConfigManager::Save - Failed to set permissions (0600) on: " + path + ". Error: " + GetLastErrorMessage());
    } else {
        LOG("ConfigManager::Save - Set permissions to 0600 on: " + path);
    }
    #endif
}
