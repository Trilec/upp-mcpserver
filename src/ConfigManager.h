#pragma once

#include <Core/Core.h>
#include "McpServer.h" // It seems McpServer.h is in ../include not here

// Correcting the include path for McpServer.h
// #include "McpServer.h" should be #include "../include/McpServer.h" if McpServer.h is in include/
// However, typical U++ project structure might handle this with include paths.
// For now, assuming the build system will find McpServer.h from the include directory.
// If not, this path will need adjustment: e.g. #include "include/McpServer.h" or similar
// For a file in src/ to include a file in include/, it's often "../include/McpServer.h"
// But U++ uses .upp files to define packages and their include paths.
// The design doc implies McpServer.h is a public header, so it should be findable.
// Let's assume the U++ build system handles this.

// The design brief specifies #include "McpServer.h"
// This implies that the 'include' directory is in the include path for the compiler.
// So, we will stick to the design brief.

using namespace Upp;

/**
 * @brief Holds all server configuration options.
 */
struct Config {
    Vector<String>   enabledTools;       ///< Names of tools to enable
    Permissions      permissions;        ///< Permission flags
    Vector<String>   sandboxRoots;       ///< Allowed filesystem roots
    int              serverPort       = 5000;  ///< TCP port for WebSocket server
    bool             bindAllInterfaces = false; ///< true ⇒ bind to 0.0.0.0
    int              maxLogSizeMB     = 10;    ///< Log rotation threshold
};

/**
 * @brief Load and save the `config.json` file under `<install>/config/`.
 */
class ConfigManager {
public:
    /**
     * @brief Attempt to load config from `path`.
     * @param path  Full path to config.json.
     * @param out   Populated on success.
     * @return true if loaded and parsed successfully; false otherwise.
     */
    static bool Load(const String& path, Config& out);

    /**
     * @brief Save `cfg` to `path` as JSON.
     *        Overwrites existing file; sets file permissions to owner-only.
     */
    static void Save(const String& path, const Config& cfg);
};
