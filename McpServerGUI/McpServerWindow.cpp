/**
 * @file McpServerWindow.cpp
 * @brief GUI window logic for MCP Server.
 */
#include "McpServerWindow.h"
#include "../include/McpServer.h"
#include <mcp_server_lib/ConfigManager.h> // For Config struct & ConfigManager class
#include "McpSplash.h"
#include <CtrlLib/CtrlLib.h>

using namespace Upp;

McpServerWindow::McpServerWindow(McpServer& server, Config& cfg)
    : server_ref(server), current_config_ref(cfg)
{
    CtrlLayoutOK(*this);
    Title("MCP Server Control Panel");

    // Initialize UI from Config (includes calling RefreshToolLists indirectly)
    SyncConfigToUI();

    // Sidebar navigation
    btnMenu.WhenAction = THISBACK(MenuAction);
    btnToolsIcon.WhenAction = THISBACK(ShowToolsPanel);
    btnConfigIcon.WhenAction = THISBACK(ShowConfigPanel);
    btnPermsIcon.WhenAction = THISBACK(ShowPermsPanel);
    btnSandboxIcon.WhenAction = THISBACK(ShowSandboxPanel);
    btnLogsIcon.WhenAction = THISBACK(ShowLogsPanel);

    // Main actions
    btnStart.WhenAction = THISBACK(OnStartServer);
    btnStop.WhenAction = THISBACK(OnStopServer);

    // Config Panel direct actions
    portEdit.WhenEnter << THISBACK(UpdateConfigFromPortEdit);
    bindList.WhenAction << THISBACK(UpdateConfigFromBindList);

    // Permissions Panel direct actions
#define WIRE_PERM_CHECKBOX(NAME, FIELD) \
    NAME.WhenAction = [this]() { current_config_ref.permissions.FIELD = NAME.Get(); AppendLog(String(#FIELD) + (NAME.Get() ? " enabled" : " disabled") + ".\nConfig updated."); }
    WIRE_PERM_CHECKBOX(chkRead, allowReadFiles); WIRE_PERM_CHECKBOX(chkWrite, allowWriteFiles);
    WIRE_PERM_CHECKBOX(chkDelete, allowDeleteFiles); WIRE_PERM_CHECKBOX(chkRename, allowRenameFiles);
    WIRE_PERM_CHECKBOX(chkCreateDir, allowCreateDirs); WIRE_PERM_CHECKBOX(chkSearch, allowSearchDirs);
    WIRE_PERM_CHECKBOX(chkExec, allowExec); WIRE_PERM_CHECKBOX(chkNetwork, allowNetworkAccess);
    WIRE_PERM_CHECKBOX(chkExternal, allowExternalStorage); WIRE_PERM_CHECKBOX(chkAttr, allowChangeAttributes);
    WIRE_PERM_CHECKBOX(chkIPC, allowIPC);
#undef WIRE_PERM_CHECKBOX

    // Sandbox panel actions
    btnAddRoot.WhenAction = THISBACK(AddSandboxRootAction);
    btnRemoveRoot.WhenAction = THISBACK(RemoveSandboxRootAction);
    sandboxRootsList.WhenBar = THISBACK(SandboxListMenu);

    // Logs panel actions
    btnClearLogs.WhenAction = THISBACK(ClearLogsAction);
    maxLogSizeEdit.WhenEnter << THISBACK(UpdateConfigFromMaxLogSize);

    // Tool List Actions (Double Click)
    toolsAvailable.WhenLeftDouble = THISBACK(ToolEnableAction);
    toolsEnabled.WhenLeftDouble = THISBACK(ToolDisableAction);

    SetEditingState(true);
    MainStack.Set(ToolsPanel);
    UpdateStatusDisplay();

    String exePath = GetExeFolder();
    lblInstallPath = "Install: " + exePath;
    if(HasChild(&lblInstallPathConfig)) {
        lblInstallPathConfig = "Install Path: " + exePath;
    } else {
        LOG("Warning: lblInstallPathConfig control not found in McpServerWindow layout.");
    }
}

void McpServerWindow::OnStartServer() {
    AppendLog("Start Server button clicked.\n");
    SyncUIToConfig();
    AppendLog("Configuration object updated from UI settings.\n");
    String configFilePath = NormalizePath(AppendFileName(GetExeFolder(), "config/config.json"));
    RealizeDirectory(GetFileFolder(configFilePath));
    ConfigManager::Save(configFilePath, current_config_ref);
    AppendLog("Configuration saved to: " + configFilePath + "\n");
    if(server_ref.logCallback) server_ref.logCallback("Configuration saved by GUI to " + configFilePath);
    server_ref.SetPort(current_config_ref.serverPort);
    server_ref.ConfigureBind(current_config_ref.bindAllInterfaces);
    server_ref.GetPermissions() = current_config_ref.permissions;
    server_ref.GetSandboxRoots().Clear();
    for(const String& root : current_config_ref.sandboxRoots) { server_ref.AddSandboxRoot(root); }
    Vector<String> allKnownTools = server_ref.GetAllToolNames();
    for(const String& toolName : allKnownTools) {
        if(current_config_ref.enabledTools.Find(toolName) >= 0) { server_ref.EnableTool(toolName); }
        else { server_ref.DisableTool(toolName); }
    }
    AppendLog("Server instance configured with current settings.\n");
    McpSplash splash(*this, server_ref, current_config_ref);
    splash.Run(true);
    if (server_ref.StartServer()) {
        AppendLog("McpServer reported STARTED successfully.\n");
        SetEditingState(false);
    } else {
        AppendLog("McpServer reported FAILED to start.\n");
        Exclamation("Failed to start MCP server. Check logs for details.");
    }
    UpdateStatusDisplay();
}

void McpServerWindow::OnStopServer() {
    AppendLog("Stop Server button clicked.\n");
    server_ref.StopServer();
    AppendLog("McpServer StopServer() called.\n");
    SetEditingState(true);
    UpdateStatusDisplay();
}

void McpServerWindow::SetEditingState(bool enabled) {
    btnStart.Enable(enabled); btnStop.Enable(!enabled);
    ConfigPanel.Enable(enabled); PermsPanel.Enable(enabled);
    SandboxPanel.Enable(enabled); maxLogSizeEdit.Enable(enabled);
    toolsAvailable.ReadOnly(!enabled); toolsEnabled.ReadOnly(!enabled);
    // Also enable/disable context menus or double-click actions if needed
    // For ArrayCtrl, ReadOnly typically prevents selection changes by user,
    // but programmatic changes are still possible. Actual interaction might need more.
    // For now, this should be sufficient to prevent user edits of the lists when server is running.
}

void McpServerWindow::AppendLog(const String& text) {
    Time t = GetSysTime();
    String timestamped_line = Format("%02d:%02d:%02d ", t.hour, t.minute, t.second) + text;
    logConsole.Insert(logConsole.GetLength(), timestamped_line);
    if (!timestamped_line.EndsWith("\n")) { logConsole.Insert(logConsole.GetLength(), "\n"); }
    logConsole.Scrolly(logConsole.GetLineCount() * Draw::GetStdFontCy());
    if (logConsole.GetLineCount() > 1000) { logConsole.Remove(0, logConsole.GetLineCount() - 500); }
}

void McpServerWindow::UpdateStatusDisplay() {
    if (server_ref.IsListening()) {
        // The minimal server implementation does not provide GetListenHost().
        // Determine the host from our bind setting instead.
        String actual_host_display = server_ref.GetBindAllInterfaces() ? "0.0.0.0" : "127.0.0.1";
        lblStatus = "Status: Running on " + actual_host_display + ":" + AsString(server_ref.GetPort());
    } else {
        lblStatus = "Status: Stopped";
    }
}

void McpServerWindow::MenuAction() { AppendLog("Menu button clicked.\n"); }
void McpServerWindow::ShowToolsPanel()    { MainStack.Set(ToolsPanel); AppendLog("Navigated to Tools panel.\n"); }
void McpServerWindow::ShowConfigPanel()   { MainStack.Set(ConfigPanel); AppendLog("Navigated to Config panel.\n"); }
void McpServerWindow::ShowPermsPanel()    { MainStack.Set(PermsPanel); AppendLog("Navigated to Permissions panel.\n"); }
void McpServerWindow::ShowSandboxPanel()  { MainStack.Set(SandboxPanel); AppendLog("Navigated to Sandbox panel.\n"); }
void McpServerWindow::ShowLogsPanel()     { MainStack.Set(LogsPanel); AppendLog("Navigated to Logs panel.\n"); }

void McpServerWindow::UpdateConfigFromPortEdit() {
    if (portEdit.IsModified()) {
        current_config_ref.serverPort = portEdit.GetData();
        AppendLog("UI Updated: Server port set to: " + AsString(current_config_ref.serverPort) + ".\nConfig updated.");
        portEdit.ClearModify();
    }
}
void McpServerWindow::UpdateConfigFromBindList() {
    bool new_bind_all = bindList.GetIndex() == 1;
    if (current_config_ref.bindAllInterfaces != new_bind_all) {
        current_config_ref.bindAllInterfaces = new_bind_all;
        AppendLog(String("UI Updated: Bind to all interfaces set to: ") + (current_config_ref.bindAllInterfaces ? "Yes" : "No") + ".\nConfig updated.");
    }
}
void McpServerWindow::UpdateConfigFromMaxLogSize() {
     if (maxLogSizeEdit.IsModified()) {
        current_config_ref.maxLogSizeMB = maxLogSizeEdit.GetData();
        AppendLog("UI Updated: Max log size set to: " + AsString(current_config_ref.maxLogSizeMB) + " MB.\nConfig updated.");
        maxLogSizeEdit.ClearModify();
    }
}

void McpServerWindow::AddSandboxRootAction() {
    FileSelector fs; fs.Multi(false); fs.BrowseDirectories(true); fs.NoBrowseFiles(true);
    fs.Title("Select Sandbox Root Directory");
    if (!current_config_ref.sandboxRoots.IsEmpty()) { fs.Set(GetFileFolder(current_config_ref.sandboxRoots[0])); }
    else { fs.Set(GetHomeDirectory()); }
    if (fs.ExecuteSelectDir()) {
        String newRoot = NormalizePath(fs.Get());
        if (!newRoot.IsEmpty()) {
            if (current_config_ref.sandboxRoots.Find(newRoot) < 0) {
                current_config_ref.sandboxRoots.Add(newRoot);
                RefreshSandboxList();
                AppendLog("Added sandbox root: " + newRoot + ". Config updated.\n");
            } else { AppendLog("Sandbox root already exists: " + newRoot + ".\n"); }
        }
    }
}
void McpServerWindow::RemoveSandboxRootAction() {
    int ix = sandboxRootsList.GetCursor();
    if (ix >= 0) {
        String removedRoot = sandboxRootsList.Get(ix, 0).ToString();
        current_config_ref.sandboxRoots.RemoveKey(removedRoot);
        RefreshSandboxList();
        AppendLog("Removed sandbox root: " + removedRoot + ". Config updated.\n");
    } else { AppendLog("No sandbox root selected to remove.\n"); }
}
void McpServerWindow::SandboxListMenu(Bar& bar) {
    if(sandboxRootsList.IsCursor() && !sandboxRootsList.IsEmpty()) {
        bar.Add("Remove Selected Root", THISBACK(RemoveSandboxRootAction));
    }
    bar.Separator();
    bar.Add("Add New Root...", THISBACK(AddSandboxRootAction));
}
void McpServerWindow::ClearLogsAction() { logConsole.Clear(); AppendLog("Log display cleared by user.\n"); }

void McpServerWindow::SyncConfigToUI() {
    AppendLog("SyncConfigToUI: Loading configuration into UI elements.\n");
    portEdit <<= current_config_ref.serverPort; portEdit.ClearModify();
    bindList.SetIndex(current_config_ref.bindAllInterfaces ? 1 : 0);
    maxLogSizeEdit <<= current_config_ref.maxLogSizeMB; maxLogSizeEdit.ClearModify();
    RefreshPermissionCheckboxes(); RefreshSandboxList(); RefreshToolLists();
}

void McpServerWindow::SyncUIToConfig() {
    AppendLog("SyncUIToConfig: Ensuring configuration object matches UI state.\n");
    current_config_ref.serverPort = portEdit.GetData();
    current_config_ref.bindAllInterfaces = bindList.GetIndex() == 1;
    current_config_ref.maxLogSizeMB = maxLogSizeEdit.GetData();
    // Permissions, SandboxRoots, EnabledTools are updated directly by their respective handlers/actions.
}

void McpServerWindow::RefreshToolLists() {
    AppendLog("RefreshToolLists: Populating tool lists.\n");
    toolsAvailable.Clear();
    toolsEnabled.Clear();

    Vector<String> allRegisteredTools = server_ref.GetAllToolNames();
    // Ensure tools in enabledTools list actually exist in registered tools
    // This also handles if a tool was removed from server registration but still in config.
    Vector<String> validatedEnabledTools;
    for(const String& enabledToolName : current_config_ref.enabledTools) {
        if (allRegisteredTools.Find(enabledToolName) >= 0) {
            validatedEnabledTools.Add(enabledToolName);
        } else {
            AppendLog("Warning: Tool '" + enabledToolName + "' is in config's enabled list but not registered with the server. It will be ignored.\n");
        }
    }
    // Update the config object with the validated list of enabled tools.
    // This ensures that if the config file had stale entries, they are cleaned up here.
    // current_config_ref.enabledTools = pick(validatedEnabledTools); // pick() is for efficiency if validatedEnabledTools is temporary
    current_config_ref.enabledTools = clone(validatedEnabledTools); // clone() if validatedEnabledTools is used later or if pick is not desired.

    for (const String& toolName : allRegisteredTools) {
        if (current_config_ref.enabledTools.Find(toolName) >= 0) {
            toolsEnabled.Add(toolName); // Column 0 is default for Add(Value)
        } else {
            toolsAvailable.Add(toolName);
        }
    }
    // Add headers if ArrayCtrl was set up with them (e.g. toolsAvailable.Header(); toolsAvailable.AddColumn("Tool Name"); )
    // For simple lists, just Add() is fine. The .layout file specifies "columns='Tool Name'".
    // This means the ArrayCtrl expects one column. Add(Value) adds a row with Value in first column.
}

void McpServerWindow::RefreshSandboxList() {
    AppendLog("Populating sandbox roots list from config.\n");
    sandboxRootsList.Clear();
    for(const String& root : current_config_ref.sandboxRoots) {
        sandboxRootsList.Add(NormalizePath(root));
    }
}

void McpServerWindow::RefreshPermissionCheckboxes() {
    AppendLog("Setting permission checkboxes from config.\n");
#define APPLY_PERM_CHECKBOX(NAME, FIELD) NAME.Set(current_config_ref.permissions.FIELD)
    APPLY_PERM_CHECKBOX(chkRead, allowReadFiles); APPLY_PERM_CHECKBOX(chkWrite, allowWriteFiles);
    APPLY_PERM_CHECKBOX(chkDelete, allowDeleteFiles); APPLY_PERM_CHECKBOX(chkRename, allowRenameFiles);
    APPLY_PERM_CHECKBOX(chkCreateDir, allowCreateDirs); APPLY_PERM_CHECKBOX(chkSearch, allowSearchDirs);
    APPLY_PERM_CHECKBOX(chkExec, allowExec); APPLY_PERM_CHECKBOX(chkNetwork, allowNetworkAccess);
    APPLY_PERM_CHECKBOX(chkExternal, allowExternalStorage); APPLY_PERM_CHECKBOX(chkAttr, allowChangeAttributes);
    APPLY_PERM_CHECKBOX(chkIPC, allowIPC);
#undef APPLY_PERM_CHECKBOX
}

void McpServerWindow::RefreshLogConfig() {
    maxLogSizeEdit <<= current_config_ref.maxLogSizeMB;
    maxLogSizeEdit.ClearModify();
}

// --- Tool List Management Actions ---
void McpServerWindow::ToolEnableAction() { // Called on DblClick on toolsAvailable
    if (!toolsAvailable.IsCursor()) return;
    String toolName = toolsAvailable.GetCurrentRow().Get(0).ToString(); // Get tool name from selected row, col 0

    // Move from available to enabled
    toolsAvailable.RemoveCursor(); // Remove from current list
    toolsEnabled.Add(toolName);    // Add to other list

    // Update config
    if (current_config_ref.enabledTools.Find(toolName) < 0) { // Add if not already there (shouldn't be)
        current_config_ref.enabledTools.Add(toolName);
    }
    AppendLog("Tool '" + toolName + "' enabled. Config updated.\n");
    // Note: If server is running, this change only affects config until server is restarted
    // or if server has a dynamic way to enable/disable tools (not in current minimal server).
}

void McpServerWindow::ToolDisableAction() { // Called on DblClick on toolsEnabled
    if (!toolsEnabled.IsCursor()) return;
    String toolName = toolsEnabled.GetCurrentRow().Get(0).ToString();

    // Move from enabled to available
    toolsEnabled.RemoveCursor();
    toolsAvailable.Add(toolName);

    // Update config
    current_config_ref.enabledTools.RemoveKey(toolName); // RemoveKey is fine for Vector<String>
    AppendLog("Tool '" + toolName + "' disabled. Config updated.\n");
}
