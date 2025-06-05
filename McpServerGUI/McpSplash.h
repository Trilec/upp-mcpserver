#ifndef _McpSplash_h_
#define _McpSplash_h_

#include <CtrlLib/CtrlLib.h>

#define LAYOUTFILE "McpSplash.layout"
#include <CtrlCore/lay.h>

// Project-specific includes needed for McpSplash definition
#include <mcp_server_lib/McpServer.h>   // For McpServer class
#include <mcp_server_lib/ConfigManager.h> // For Config struct (though McpServer.h might bring Permissions if Config uses it)
                                        // McpSplash constructor takes const Config&

using namespace Upp;

class McpSplash : public WithMcpSplashLayout<TopWindow> {
public:
    typedef McpSplash CLASSNAME;

    // Constructor takes owner window (usually main window), McpServer, and Config
    McpSplash(TopWindow& owner, McpServer& server, const Config& config);
    ~McpSplash();

    void Run(bool modal = true); // To show the splash screen

private:
    void OnProceed();
    void OnTimeout();
    void UpdateInfoText(); // Populates the text from server/config

    // References to server and config to display their info
    McpServer& server_ref;
    const Config& config_ref;

    Timer  autoCloseTimer; // For auto-closing the splash screen
};

#endif
