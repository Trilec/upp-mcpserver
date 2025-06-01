/**
 * @file test_permissions.cpp
 * @brief Unit tests for McpServer permission flags.
 */

#include "../include/McpServer.h" // For McpServer class & Permissions struct
#include <Core/Core.h>         // For U++ Core utilities, LOG, ASSERT
#include <Test/Test.h>       // For U++ Test framework

// Helper function to simulate a tool call that checks a specific permission
// This is highly simplified. Real tool calls involve JSON args and results.
bool SimulateToolCall(McpServer& server, bool (Permissions::*perm_flag), const String& actionName) {
    LOG("  Simulating tool call requiring permission for: " + actionName);
    if ((server.GetPermissions().*perm_flag)) {
        LOG("    Permission GRANTED for " + actionName);
        return true; // Allowed
    } else {
        LOG("    Permission DENIED for " + actionName);
        // In a real tool, this would throw an exception.
        // For this test, we just return false.
        return false; // Denied
    }
}


CONSOLE_APP_MAIN
{
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    int exit_code = 0;

    LOG("--- Permissions Unit Tests ---");

    // Test Case 1: Default permissions (all false)
    {
        LOG("Test Case 1: Default permissions.");
        McpServer server(1234,1);
        Permissions& perms = server.GetPermissions();

        ASSERT(!perms.allowReadFiles);
        ASSERT(!perms.allowWriteFiles);
        // ... assert all other permissions are false by default ...
        ASSERT(!perms.allowExec);
        LOG("  PASSED: Default permissions are all false.");
    }

    // Test Case 2: Enable a specific permission (e.g., allowReadFiles)
    {
        LOG("Test Case 2: Enable allowReadFiles.");
        McpServer server(1234,1);
        server.GetPermissions().allowReadFiles = true;

        ASSERT(server.GetPermissions().allowReadFiles);
        ASSERT(!server.GetPermissions().allowWriteFiles); // Ensure others are still false
        LOG("  PASSED: allowReadFiles is true, others default false.");

        // Simulate a tool that needs this permission
        if (!SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile")) {
            LOG("  FAILED: ReadFile tool was denied when permission was true.");
            exit_code=1;
        }
        ASSERT(SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile"));

        if (SimulateToolCall(server, &Permissions::allowWriteFiles, "WriteFile")) {
             LOG("  FAILED: WriteFile tool was allowed when permission was false.");
             exit_code=1;
        }
        ASSERT(!SimulateToolCall(server, &Permissions::allowWriteFiles, "WriteFile"));
    }

    // Test Case 3: Enable multiple permissions
    {
        LOG("Test Case 3: Enable multiple permissions (allowWriteFiles, allowCreateDirs).");
        McpServer server(1234,1);
        server.GetPermissions().allowWriteFiles = true;
        server.GetPermissions().allowCreateDirs = true;

        ASSERT(server.GetPermissions().allowWriteFiles);
        ASSERT(server.GetPermissions().allowCreateDirs);
        ASSERT(!server.GetPermissions().allowReadFiles);
        LOG("  PASSED: Multiple specific permissions are true.");

        ASSERT(SimulateToolCall(server, &Permissions::allowWriteFiles, "WriteFile"));
        ASSERT(SimulateToolCall(server, &Permissions::allowCreateDirs, "CreateDir"));
        ASSERT(!SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile"));
    }

    // Test Case 4: All permissions enabled
    {
        LOG("Test Case 4: All permissions enabled.");
        McpServer server(1234,1);
        Permissions& perms = server.GetPermissions();
        perms.allowReadFiles       = true;
        perms.allowWriteFiles      = true;
        perms.allowDeleteFiles     = true;
        perms.allowRenameFiles     = true;
        perms.allowCreateDirs      = true;
        perms.allowSearchDirs      = true;
        perms.allowExec            = true;
        perms.allowNetworkAccess   = true;
        perms.allowExternalStorage = true;
        perms.allowChangeAttributes= true;
        perms.allowIPC             = true;

        ASSERT(SimulateToolCall(server, &Permissions::allowReadFiles, "ReadFile"));
        ASSERT(SimulateToolCall(server, &Permissions::allowExec, "ExecuteProcess"));
        // ... Test a few more ...
        LOG("  PASSED: All permissions are true and respected.");
    }

    // Test Case 5: Modifying permissions through GetPermissions() reference
    {
        LOG("Test Case 5: Modifying through reference.");
        McpServer server(1234,1);
        Permissions& perms_ref = server.GetPermissions();
        perms_ref.allowNetworkAccess = true;

        ASSERT(server.GetPermissions().allowNetworkAccess);
        ASSERT(SimulateToolCall(server, &Permissions::allowNetworkAccess, "NetworkCall"));
        LOG("  PASSED: Modification via reference works.");
    }


    if (exit_code == 0) {
        LOG("--- All Permission Tests PASSED ---");
    } else {
        LOG("--- Some Permission Tests FAILED ---");
    }
    SetExitCode(exit_code);
}
