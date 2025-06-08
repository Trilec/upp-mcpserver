#include "../include/McpServer.h"
#include <Core/Core.h>
#include "test_helpers.h"

bool SimulateToolCall(McpServer& server, bool (Permissions::*perm_flag), const String& action)
{
    return (server.GetPermissions().*perm_flag);
}

TEST(Permissions_DefaultFlagsFalse)
{
    McpServer server(1234,1);
    const Permissions& p = server.GetPermissions();
    ASSERT(!p.allowReadFiles && !p.allowWriteFiles && !p.allowExec);
}

TEST(Permissions_EnableSpecific)
{
    McpServer server(1234,1);
    server.GetPermissions().allowReadFiles = true;
    ASSERT(server.GetPermissions().allowReadFiles);
    ASSERT(!server.GetPermissions().allowWriteFiles);
    ASSERT(SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile"));
    ASSERT(!SimulateToolCall(server, &Permissions::allowWriteFiles, "WriteFile"));
}

TEST(Permissions_EnableMultiple)
{
    McpServer server(1234,1);
    server.GetPermissions().allowWriteFiles = true;
    server.GetPermissions().allowCreateDirs = true;
    ASSERT(server.GetPermissions().allowWriteFiles);
    ASSERT(server.GetPermissions().allowCreateDirs);
    ASSERT(!server.GetPermissions().allowReadFiles);
    ASSERT(SimulateToolCall(server, &Permissions::allowWriteFiles, "WriteFile"));
    ASSERT(SimulateToolCall(server, &Permissions::allowCreateDirs, "CreateDir"));
    ASSERT(!SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile"));
}

TEST(Permissions_AllEnabled)
{
    McpServer server(1234,1);
    Permissions& perms = server.GetPermissions();
    perms.allowReadFiles=true; perms.allowWriteFiles=true; perms.allowDeleteFiles=true;
    perms.allowRenameFiles=true; perms.allowCreateDirs=true; perms.allowSearchDirs=true;
    perms.allowExec=true; perms.allowNetworkAccess=true; perms.allowExternalStorage=true;
    perms.allowChangeAttributes=true; perms.allowIPC=true;
    ASSERT(SimulateToolCall(server, &Permissions::allowReadFiles, "Read"));
    ASSERT(SimulateToolCall(server, &Permissions::allowExec, "Exec"));
}

TEST(Permissions_ModifyViaReference)
{
    McpServer server(1234,1);
    Permissions& ref = server.GetPermissions();
    ref.allowNetworkAccess = true;
    ASSERT(server.GetPermissions().allowNetworkAccess);
    ASSERT(SimulateToolCall(server, &Permissions::allowNetworkAccess, "Network"));
}
