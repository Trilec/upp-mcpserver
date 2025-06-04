#ifndef _McpSplash_h_
#define _McpSplash_h_

#include <CtrlLib/CtrlLib.h>
#define LAYOUTFILE "McpSplash.layout"
#include <CtrlCore/lay.h>

#include "../include/McpServer.h"         // Corrected path
#include <mcp_server_lib/ConfigManager.h> // Corrected path (package include)

using namespace Upp;

class McpSplash : public WithMcpSplashLayout<TopWindow> {
public:
    typedef McpSplash CLASSNAME;
    McpSplash(TopWindow& owner, McpServer& server, const Config& config);
    ~McpSplash();
    void Run(bool modal = true);
private:
    void OnProceed(); void OnTimeout(); void UpdateInfoText();
    McpServer& server_ref;
    const Config& config_ref;
    Timer  autoCloseTimer;
};
#endif
