/**
 * @file McpSplash.cpp
 * @brief Splash screen behavior for MCP Server.
 */
#include "McpSplash.h"
#include "../include/McpServer.h" // For McpServer class (mainly for config_ref context) and Permissions struct
#include "ConfigManager.h"     // For Config struct (via McpServer.h or directly if needed)

McpSplash::McpSplash(TopWindow& owner, McpServer& server, const Config& config)
    : server_ref(server), config_ref(config) // Store references
{
    CtrlLayoutOK(*this); // Load layout from McpSplash.layout
    // SetOwner(&owner); // This makes it application modal relative to owner, which is good.
                       // However, TopWindow::RunAppModal() called in Run() makes it fully app modal.
                       // If just Open() is used, SetOwner is important.
    Title("MCP Server Status"); // Set a title for the splash window
    // CenterInOwner(); // Good practice to center over the main window if SetOwner is used.

    UpdateInfoText(); // Populate the text display

    btnProceed.WhenAction = THISBACK(OnProceed); // Wire the "Proceed" button

    // Timer is initialized here but started in Run()
}

McpSplash::~McpSplash()
{
    autoCloseTimer.Kill(); // Important: stop the timer if the window is destroyed.
}

void McpSplash::Run(bool modal)
{
    // Start the timer for auto-close when Run is called.
    // SetQT is for one-shot timers. 3000 milliseconds = 3 seconds.
    autoCloseTimer.SetQT(3000, THISBACK(OnTimeout));

    if (modal) {
        // Make sure it's centered if being run modally.
        // If 'owner' was passed and SetOwner was called, CenterInOwner() is preferred.
        // Otherwise, CenterScreen().
        CenterScreen();
        TopWindow::RunAppModal(); // Standard U++ modal execution. Blocks until Close().
    } else {
        // For non-modal, just open. Might need different positioning logic.
        // Non-modal splash screens are less common for this kind of notice.
        // CenterScreen(); // Or some other appropriate positioning
        Open();
    }
}

void McpSplash::UpdateInfoText()
{
    StringStream ss; // Use StringStream to build the display text

    // Application Name and Version (example)
    ss << "MCP Server v0.1.0 (Alpha)\n"; // TODO: Make version dynamic
    ss << "Copyright (c) YYYY [Your Name Here]\n\n"; // TODO: Update copyright year/name

    // Active Permissions
    ss << "**Current Permissions:**\n";
    const Permissions& perms = config_ref.permissions; // Get permissions from the passed config
    bool no_perms = true;
    if (perms.allowReadFiles)       { ss << "  - Read Files\n"; no_perms = false; }
    if (perms.allowWriteFiles)      { ss << "  - Write Files\n"; no_perms = false; }
    if (perms.allowDeleteFiles)     { ss << "  - Delete Files\n"; no_perms = false; }
    if (perms.allowRenameFiles)     { ss << "  - Rename/Move Files\n"; no_perms = false; }
    if (perms.allowCreateDirs)      { ss << "  - Create Directories\n"; no_perms = false; }
    if (perms.allowSearchDirs)      { ss << "  - Search Directories\n"; no_perms = false; }
    if (perms.allowExec)            { ss << "  - Execute Processes [!]\n"; no_perms = false; }
    if (perms.allowNetworkAccess)   { ss << "  - Network Access [!]\n"; no_perms = false; }
    if (perms.allowExternalStorage) { ss << "  - External Storage Access\n"; no_perms = false; }
    if (perms.allowChangeAttributes){ ss << "  - Change File Attributes\n"; no_perms = false; }
    if (perms.allowIPC)             { ss << "  - Inter-Process Communication\n"; no_perms = false; }
    if (no_perms) {
        ss << "  (None - Server has minimal capabilities by default)\n";
    }
    ss << "\n";

    // Sandbox Roots
    ss << "**Sandbox Roots:**\n";
    if (config_ref.sandboxRoots.IsEmpty()) {
        ss << "  (None defined - File operations may be unrestricted or fail if tools expect sandboxing!) [WARNING]\n";
    } else {
        for (const String& root : config_ref.sandboxRoots) {
            ss << "  - " << root << "\n";
        }
    }
    ss << "\n";

    // Critical Warnings (Consolidated)
    ss << "**Important Warnings:**\n";
    bool has_warnings = false;
    if (config_ref.sandboxRoots.IsEmpty()) {
        ss << "  - No sandbox roots: File tools might have unrestricted access or fail if they require a sandbox.\n";
        has_warnings = true;
    }
    if (perms.allowExec) {
        ss << "  - Execution of external processes is ENABLED. This is a high-risk permission.\n";
        has_warnings = true;
    }
    if (perms.allowNetworkAccess) {
        ss << "  - Network access for tools is ENABLED. Ensure this is intended.\n";
        has_warnings = true;
    }
    if (!has_warnings) {
        ss << "  (No critical warnings based on current settings.)\n";
    }

    // Apply the text to the StaticText control from the layout
    txtInfo = ss;
    // For QTF (rich text) formatting, you might use:
    // txtInfo.SetQTF(QTF_TEXT(ss.GetResult())); // Assuming ss contains QTF compatible markup
}

void McpSplash::OnProceed()
{
    autoCloseTimer.Kill(); // Stop the timer if "Proceed" is clicked manually
    Close();               // Close the splash screen dialog
}

void McpSplash::OnTimeout()
{
    // This method is called by the autoCloseTimer when the timeout occurs
    if (IsOpen()) { // Check if the window is still open (it should be)
        Close(); // Close the splash screen
    }
}
