# U++ MCP Server (upp-mcpserver)

U++ MCP Server is a server application built with the [U++ framework](https://www.ultimatepp.org/). It allows AI clients (or other programs) to call modular "tools" via a WebSocket interface using a JSON-based Message Communication Protocol (MCP). The server features a plugin-style architecture for tools, fine-grained permissions, sandboxing capabilities, and a rich GUI for configuration and monitoring.

## Features

- **WebSocket Interface**: Exposes tools over WebSockets using a simple JSON protocol.
- **Tool Manifest**: Provides a JSON manifest of available tools, their descriptions, and parameter schemas.
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
- `/examples`: Standalone example applications demonstrating how to implement and register different tools. Each example runs its own McpServer instance on a separate port.

## Getting Started

### Prerequisites

- U++ (Ultimate++) framework installed. (See [U++ download](https://www.ultimatepp.org/www$uppweb$download$en-us.html) and [TheIDE setup](https://www.ultimatepp.org/app$ide$tutorial$en-us.html)).
- A C++11 (or newer) compatible compiler configured within TheIDE (e.g., GCC, Clang, MSVC).

### Building and Running (General Outline)

**Using TheIDE (Recommended for U++)**:

1.  **Open TheIDE**.
2.  Create a new **main package** (assembly) named `upp-mcpserver` (or open if `.upp` files are provided later).
3.  Add the following sub-packages (or configure them within the main package):
    *   A **StaticLibrary** package (e.g., `mcp_server_lib`) containing:
        *   `include/McpServer.h`
        *   `src/McpServer.cpp`
        *   `src/ConfigManager.h`, `src/ConfigManager.cpp`
        *   It should depend on U++ packages: `Core`, `Json`, `WebSockets`.
    *   An **Executable (CtrlLib)** package for the main GUI application (e.g., `McpServerGUI`):
        *   `src/Main.cpp`
        *   `src/McpServerWindow.h`, `src/McpServerWindow.cpp`
        *   `src/McpSplash.h`, `src/McpSplash.cpp`
        *   `ui/McpServerWindow.layout`, `ui/McpSplash.layout` (and icon resources from `ui/icons/`)
        *   It should depend on `mcp_server_lib` and U++ packages: `Core`, `CtrlLib`.
    *   For each example in `examples/`:
        *   An **Executable (Console)** package (e.g., `FileReaderExample`):
            *   `examples/file_reader/file_reader_main.cpp`
            *   It should depend on `mcp_server_lib` and U++ packages: `Core`, `Json`, `WebSockets`.
4.  Set the main package to `upp-mcpserver`.
5.  Select the `McpServerGUI` executable as the build target.
6.  **Build and Run** (e.g., by pressing F5 or using the build menu).

**Using CMake (Placeholder - Detailed U++ CMake setup is complex)**:

A full CMake build system for U++ projects requires proper handling of U++'s package system and code generation (e.g., from `.layout` files). The provided `CMakeLists.txt` files are basic placeholders.
```bash
# Placeholder commands - actual U++ CMake build is more involved
# mkdir build && cd build
# cmake ..
# make
# ./McpServerGUI
```

### First Run

- On the first run of `McpServerGUI.exe` (or equivalent), if `/config/config.json` does not exist, it will be created with default settings.
- The GUI will appear. You can configure tools, permissions, sandbox roots, and server settings.
- Click "Start Server". A splash screen will show key settings and warnings.
- The server will start listening on the configured port (default: 5000).

## Using the MCP Server

Clients connect via WebSockets (e.g., `ws://localhost:5000/`).

1.  **On Connection**: The server sends a "manifest" message:
    ```json
    {
      "type": "manifest",
      "tools": {
        "tool_name_1": { "description": "...", "parameters": { /* schema */ } },
        "tool_name_2": { "description": "...", "parameters": { /* schema */ } }
      }
    }
    ```

2.  **Calling a Tool**: The client sends a "tool_call" message:
    ```json
    {
      "type": "tool_call",
      "tool": "tool_name_to_call",
      "args": { /* arguments matching the tool's parameter schema */ }
    }
    ```

3.  **Receiving a Response**:
    - Success:
      ```json
      {
        "type": "tool_response",
        "result": { /* tool's JSON result */ }
      }
      ```
    - Error:
      ```json
      {
        "type": "error",
        "message": "Error description (e.g., permission denied, sandbox violation, tool error)"
      }
      ```

Refer to the Python client pseudocode in the design brief or `examples/python_client/client.py` (if created) for a usage example.

## Example Tools Provided

*(These are registered by `Main.cpp` in the main GUI application and also demonstrated as standalone servers in the `/examples` directory)*

-   **`read_file`**: Reads contents of a file.
    -   Permissions: `allowReadFiles`
    -   Sandbox: Yes
-   **`calculate`**: Performs basic arithmetic.
    -   Permissions: None
    -   Sandbox: No
-   **`create_dir`**: Creates a directory.
    -   Permissions: `allowCreateDirs`
    -   Sandbox: Yes
-   **`list_dir`**: Lists files and folders.
    -   Permissions: `allowSearchDirs`
    -   Sandbox: Yes
-   **`save_data`**: Writes text to a file.
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