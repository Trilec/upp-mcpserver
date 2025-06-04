#ifndef _McpServerWindow_h_
#define _McpServerWindow_h_

#include <CtrlLib/CtrlLib.h>

// This is how U++ includes the .layout file to generate the WithMyWindowLayout<> template
#define LAYOUTFILE "McpServerWindow.layout"
#include <CtrlCore/lay.h>

// Forward declarations or includes for classes used
#include "../include/McpServer.h" // For McpServer class definition
#include <mcp_server_lib/ConfigManager.h> // For Config struct definition

using namespace Upp;

class McpServerWindow : public WithMcpServerWindowLayout<TopWindow> {
public:
    typedef McpServerWindow CLASSNAME;

    McpServerWindow(McpServer& server, Config& cfg);

    void OnStartServer();
    void OnStopServer();
    void SetEditingState(bool enabled);
    void AppendLog(const String& line);

    McpServer& GetServerRef() { return server_ref; }
    Config& GetConfigRef() { return current_config_ref; }

private:
    // UI Action Handlers
    void MenuAction();
    void ShowToolsPanel();
    void ShowConfigPanel();
    void ShowPermsPanel();
    void ShowSandboxPanel();
    void ShowLogsPanel();

    void AddSandboxRootAction();
    void RemoveSandboxRootAction();
    void ClearLogsAction();

    void ToolEnableAction();  // Move tool from available to enabled
    void ToolDisableAction(); // Move tool from enabled to available

    // Internal state and UI synchronization
    void SyncConfigToUI();    // Load config values into UI widgets
    void SyncUIToConfig();    // Save UI widget values into config object

    void RefreshToolLists();
    void RefreshSandboxList();
    void RefreshPermissionCheckboxes();
    void RefreshLogConfig();
    void UpdateStatusDisplay();


    McpServer& server_ref;
    Config&    current_config_ref;
    // If config is owned by window, then: Config current_config;
    // But Main.cpp loads it, so passing by reference is likely.
};

#endif
