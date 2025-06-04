#ifndef _McpSplash_h_
#define _McpSplash_h_

#include <CtrlLib/CtrlLib.h>

#define LAYOUTFILE <McpServerGUI/McpSplash.layout>
#include <CtrlCore/lay.h>

// Forward declarations
class McpServer;
struct Config;
class TopWindow; // Parent window for modality if needed

class McpSplash : public WithMcpSplashLayout<TopWindow> {
public:
    typedef McpSplash CLASSNAME;

    McpSplash(TopWindow& owner, McpServer& server, const Config& config);
    ~McpSplash();

    void Run(bool modal = true); // Overload or parameter for modal/non-modal

private:
    void OnProceed();
    void OnTimeout();
    void UpdateInfoText();

    McpServer& server_ref;
    const Config& config_ref;
    Timer  autoCloseTimer;
};

#endif
