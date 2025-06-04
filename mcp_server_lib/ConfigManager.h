#pragma once
#include <Core/Core.h>                  // For Vector, String
// For Permissions struct. Path from mcp_server_lib/ConfigManager.h to include/McpServer.h
#include "../include/McpServer.h"

using namespace Upp;

struct Config {
    Vector<String>   enabledTools;
    Permissions      permissions;        // Needs full definition of Permissions from McpServer.h
    Vector<String>   sandboxRoots;
    uint16           serverPort       = 5000;
    bool             bindAllInterfaces = false;
    int              maxLogSizeMB     = 10;
    String           ws_path_prefix;   // Default will be set by constructor
    bool             use_tls          = false;
    String           tls_cert_path;
    String           tls_key_path;

    // Default constructor to initialize new fields like ws_path_prefix
    Config() {
        ws_path_prefix = "/mcp"; // Default path prefix
        // Other members get their default initializers from their declaration or are default-constructed.
    }
};

class ConfigManager {
public:
    static bool Load(const String& path, Config& out);
    static void Save(const String& path, const Config& cfg);
};
