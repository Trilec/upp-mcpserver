/**
 * @file McpSplash.cpp
 * @brief Splash screen behavior for MCP Server.
 */
#include "McpSplash.h"
#include "../include/McpServer.h" // For McpServer class and Permissions struct
#include "ConfigManager.h"     // For Config struct

McpSplash::McpSplash(TopWindow& owner, McpServer& server, const Config& config)
    : server_ref(server), config_ref(config)
{
    CtrlLayoutOK(*this); // Load layout
    // SetOwner(&owner); // Make it modal to the owner window passed in constructor
    Title("MCP Server Starting...");
    // CenterInOwner(); // Center it over its owner

    UpdateInfoText();

    btnProceed.WhenAction = THISBACK(OnProceed);

    // Setup timer for auto-close
    autoCloseTimer.Set(-3000, THISBACK(OnTimeout)); // 3 seconds, one-shot
                                                   // Negative delay means relative to current time for Set calls
                                                   // For initial setup, positive delay is fine.
    // autoCloseTimer.Set(3000, THISBACK(OnTimeout));
    // Let's use SetQT to ensure it's a one-shot after 3s.
    // Or manage KillSet in Run.
}

McpSplash::~McpSplash()
{
    autoCloseTimer.Kill(); // Ensure timer is stopped if window is closed prematurely
}


void McpSplash::Run(bool modal)
{
    autoCloseTimer.Set(3000, THISBACK(OnTimeout)); // Start 3s timer
    if (modal) {
        TopWindow::RunAppModal();
    } else {
        Open(); // Or OpenMain if it's the primary window (not here)
    }
}

void McpSplash::UpdateInfoText()
{
    StringStream ss;
    ss << "MCP Server v1.0.0 (Example Version)\n\n"; // Version can come from a global const or similar

    ss << "**Active Permissions:**\n";
    const Permissions& perms = config_ref.permissions; // Use config_ref as source of truth for splash
    if (!perms.allowReadFiles && !perms.allowWriteFiles && !perms.allowDeleteFiles &&
        !perms.allowRenameFiles && !perms.allowCreateDirs && !perms.allowSearchDirs &&
        !perms.allowExec && !perms.allowNetworkAccess && !perms.allowExternalStorage &&
        !perms.allowChangeAttributes && !perms.allowIPC) {
        ss << "  (None)\n";
    } else {
        if (perms.allowReadFiles)       ss << "  - Read Files\n";
        if (perms.allowWriteFiles)      ss << "  - Write Files\n";
        if (perms.allowDeleteFiles)     ss << "  - Delete Files\n";
        if (perms.allowRenameFiles)     ss << "  - Rename/Move Files\n";
        if (perms.allowCreateDirs)      ss << "  - Create Directories\n";
        if (perms.allowSearchDirs)      ss << "  - Search Directories\n";
        if (perms.allowExec)            ss << "  - Execute Processes [!]\n";
        if (perms.allowNetworkAccess)   ss << "  - Network Access [!]\n";
        if (perms.allowExternalStorage) ss << "  - External Storage\n";
        if (perms.allowChangeAttributes)ss << "  - Change Attributes\n";
        if (perms.allowIPC)             ss << "  - Inter-Process Communication\n";
    }
    ss << "\n";

    ss << "**Sandbox Roots:**\n";
    if (config_ref.sandboxRoots.IsEmpty()) {
        ss << "  (None defined - Unrestricted file access!) [!!!WARNING!!!]\n";
    } else {
        for (const String& root : config_ref.sandboxRoots) {
            ss << "  - " << root << "\n";
        }
    }
    ss << "\n";

    ss << "**Critical Warnings:**\n";
    bool hasWarnings = false;
    if (config_ref.sandboxRoots.IsEmpty()) {
        ss << "  - No sandbox roots defined! File operations are unrestricted.\n";
        hasWarnings = true;
    }
    if (perms.allowExec) {
        ss << "  - Execution of external processes is enabled.\n";
        hasWarnings = true;
    }
    if (perms.allowNetworkAccess) {
        ss << "  - Network access is enabled.\n";
        hasWarnings = true;
    }
    if (!hasWarnings) {
        ss << "  (None)\n";
    }

    txtInfo = ss; // Set the text to the StaticText control
    // txtInfo.SetQTF(ss.GetResult()); // If using QTF for rich text formatting
}

void McpSplash::OnProceed()
{
    autoCloseTimer.Kill(); // Stop timer if proceed is clicked
    Close(); // Close the splash screen
}

void McpSplash::OnTimeout()
{
    // This will be called by the timer
    if (IsOpen()) { // Check if window is still open
        Close();
    }
}
