#ifndef _McpServerWindow_h_
#define _McpServerWindow_h_

#include <CtrlLib/CtrlLib.h>

#define LAYOUTFILE "McpServerWindow.layout"
#include <CtrlCore/lay.h>

// Project-specific includes needed for McpServerWindow definition
#include <mcp_server_lib/McpServer.h>   // For McpServer class
#include <mcp_server_lib/ConfigManager.h> // For Config struct

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

    void ToolEnableAction();
    void ToolDisableAction();

    // Direct config update handlers
    void UpdateConfigFromPortEdit();
    void UpdateConfigFromBindList();
    void UpdateConfigFromMaxLogSize();

    // Internal state and UI synchronization
    void SyncConfigToUI();
    void SyncUIToConfig();

    void RefreshToolLists();
    void RefreshSandboxList();
    void RefreshPermissionCheckboxes();
    void RefreshLogConfig();
    void UpdateStatusDisplay();
    void SandboxListMenu(Bar& bar);

    McpServer& server_ref;
    Config&    current_config_ref;
};

#endif
