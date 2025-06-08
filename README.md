# U++ MCP Server (upp-mcpserver)

U++ MCP Server is a server application built with the [U++ framework](https://www.ultimatepp.org/). It allows AI clients (or other programs) to call modular "tools" via a WebSocket interface using a JSON-based Message Communication Protocol (MCP). The server features a plugin-style architecture for tools, fine-grained permissions, sandboxing capabilities, and a rich GUI for configuration and monitoring.

## Features

- **WebSocket Interface**: Exposes tools over WebSockets using a simple JSON protocol.
- **Tool Manifest**: Provides a JSON manifest of available tools (now `ums-` prefixed), their descriptions, and parameter schemas.
- **Plugin-Style Tools**: Tools can be registered in C++ code. The GUI allows enabling/disabling tools at runtime.
- **Fine-Grained Permissions**: A comprehensive set of permission flags (e.g., `allowReadFiles`, `allowExec`) to control tool capabilities.
- **Sandboxing**: Restricts file system access for tools to user-defined root directories.
- **JSON Configuration**: All settings (server port, enabled tools, permissions, sandbox roots, logging) are stored in `/config/config.json`.
- **Rich GUI (U++)**:
    - Collapsible sidebar for navigation (Tools, Configuration, Permissions, Sandbox, Logs).
    - Dual-list for managing available and enabled tools.
    - Checkboxes for all permission flags.
    - List editor for sandbox root directories.
    - Real-time log viewer with clear/export options.
    - Status bar showing server state.
    - Start/Stop server buttons.
- **Splash Screen**: Displays server status, active permissions, and warnings on startup.
- **Rolling Logs**: Detailed logging to `/config/log/mcpserver.log` with automatic rotation and compression.

## Project Structure

- `/include`: Public headers for the core server library (`McpServer.h`).
- `/src`: Core implementation files (`McpServer.cpp`, `ConfigManager.cpp`, GUI logic like `McpServerWindow.cpp`, `McpSplash.cpp`, and `Main.cpp`).
- `/ui`: U++ layout files (`.layout`) and icon resources.
- `/config`: Runtime configuration (`config.json`) and logs (`/log`).
- `/tests`: Unit test stubs.
- `/plugins`: Standalone plugin applications demonstrating how to implement and register different tools (e.g., `plugins/ums-readfile/`). Each plugin runs its own McpServer instance on a separate port.

## Getting Started

### Prerequisites

- U++ (Ultimate++) framework installed. (See [U++ download](https://www.ultimatepp.org/www$uppweb$download$en-us.html) and [TheIDE setup](https://www.ultimatepp.org/app$ide$tutorial$en-us.html)).
- A C++11 (or newer) compatible compiler configured within TheIDE (e.g., GCC, Clang, MSVC).

### Building and Running (General Outline)

**Using TheIDE (Recommended for U++)**:

1.  **Open TheIDE**.
2.  Open the main package file `upp-mcpserver.upp` (or the root project directory).
3.  TheIDE should discover the packages:
    *   `mcp_server_lib` (StaticLibrary)
    *   `McpServerGUI` (Executable GUI application)
    *   Individual plugin packages (e.g., `ums-readfile-plugin`, `ums-calc-plugin`) located in `plugins/<plugin-name>/<plugin-name>.upp`.
    *   `McpServerTests` (Test executable).
4.  Set the main package to `upp-mcpserver` (usually the one you opened).
5.  Select the `McpServerGUI` executable as the build target in TheIDE's build configuration dropdown.
6.  **Build and Run** (e.g., by pressing F5 or using the build menu).
    You can also select individual plugin executables (e.g., `ums-readfile-plugin`) as targets to build and run them independently.

**Using CMake**:

Previously the repository contained placeholder `CMakeLists.txt` files. These
files have been removed to avoid confusion. At the moment the recommended way to
build the project is with U++'s TheIDE using the provided `.upp` packages.

### First Run

- On the first run of `McpServerGUI.exe` (or equivalent), if `/config/config.json` does not exist, it will be created with default settings.
- The GUI will appear. You can configure tools, permissions, sandbox roots, and server settings.
- Click "Start Server". A splash screen will show key settings and warnings.
- The server will start listening on the configured port (default: 5000).

## Using the MCP Server

Clients connect via WebSockets (e.g., `ws://localhost:5000/`). Tool names are now prefixed (e.g., `ums-readfile`).

1.  **On Connection**: The server sends a "manifest" message:
    ```json
    {
      "type": "manifest",
      "tools": {
        "ums-readfile": { "description": "...", "parameters": { /* schema */ } },
        "ums-calc": { "description": "...", "parameters": { /* schema */ } }
        // ... other ums-prefixed tools
      }
    }
    ```

2.  **Calling a Tool**: The client sends a "tool_call" message:
    ```json
    {
      "type": "tool_call",
      "tool": "ums-readfile", // Use the new prefixed name
      "args": { /* arguments matching the tool's parameter schema */ }
    }
    ```

3.  **Receiving a Response**: (Structure remains the same)
    - Success: `{"type": "tool_response", "result": { ... }}`
    - Error: `{"type": "error", "message": "Error description"}`

Refer to the Python client pseudocode in the original design brief (remember to update tool names in client calls) or a future `plugins/python_client/client.py` for usage examples.

## Plugin Tools Provided

*(These are registered by `Main.cpp` in the main GUI application and also demonstrated as standalone servers in the `/plugins` directory. Tool names are now prefixed.)*

-   **`ums-readfile`**: Reads contents of a file.
    -   Permissions: `allowReadFiles`
    -   Sandbox: Yes
-   **`ums-calc`**: Performs basic arithmetic.
    -   Permissions: None
    -   Sandbox: No
-   **`ums-createdir`**: Creates a directory.
    -   Permissions: `allowCreateDirs`
    -   Sandbox: Yes
-   **`ums-listdir`**: Lists files and folders.
    -   Permissions: `allowSearchDirs`
    -   Sandbox: Yes
-   **`ums-writefile`**: Writes text to a file. (Formerly `save_data`)
    -   Permissions: `allowWriteFiles`
    -   Sandbox: Yes

## Contributing

(Placeholder for contribution guidelines)

1.  Fork the repository.
2.  Create a new branch (`git checkout -b feature/your-feature`).
3.  Make your changes.
4.  Commit your changes (`git commit -m 'Add some feature'`).
5.  Push to the branch (`git push origin feature/your-feature`).
6.  Open a Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.