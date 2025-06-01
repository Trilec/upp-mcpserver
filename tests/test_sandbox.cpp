/**
 * @file test_sandbox.cpp
 * @brief Unit tests for McpServer sandboxing (EnforceSandbox).
 */

#include "../include/McpServer.h" // For McpServer class
#include <Core/Core.h>         // For U++ Core utilities, LOG, ASSERT
#include <Test/Test.h>       // For U++ Test framework (TEST_CASE, EXPECT, etc.)
                             // Or whatever test framework is chosen.
                             // Using U++ Test:: syntax for this stub.

// For U++ Test framework, tests are usually grouped using TEST_SUITE
// and individual tests with TEST_CASE.
// Or using `UNITTEST` macro for simpler cases.

// Mock McpServer or use a real instance for testing EnforceSandbox
// For EnforceSandbox, we primarily need to manipulate sandboxRoots.

CONSOLE_APP_MAIN // Or GUI_APP_MAIN if tests need GUI context
{
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    int exit_code = 0;

    LOG("--- Sandbox Unit Tests ---");

    // Test Case 1: No sandbox roots defined - should allow any path
    {
        LOG("Test Case 1: No sandbox roots - any path allowed.");
        McpServer server(1234, 1); // Dummy port/clients for test
        bool exception_thrown = false;
        try {
            server.EnforceSandbox("/some/random/path/file.txt");
            server.EnforceSandbox("C:/windows/system32/dangerous.dll");
            LOG("  PASSED: No exception for unrestricted paths when no roots defined.");
        } catch (const String& e) {
            LOG("  FAILED: Exception thrown when no roots defined: " + e);
            exception_thrown = true;
            exit_code = 1;
        }
        ASSERT(!exception_thrown);
    }

    // Test Case 2: Single sandbox root - path within root
    {
        LOG("Test Case 2: Path within a single sandbox root.");
        McpServer server(1234, 1);
        String root = NormalizePath(GetExeFolder() + "/test_sandbox_area_1");
        RealizeDirectory(root); // Ensure it exists for the test
        server.AddSandboxRoot(root);
        LOG("  Added root: " + root);

        bool exception_thrown = false;
        try {
            server.EnforceSandbox(AppendFileName(root, "file_allowed.txt"));
            LOG("  PASSED: No exception for path within the root.");
        } catch (const String& e) {
            LOG("  FAILED: Exception thrown for path within root: " + e);
            exception_thrown = true;
            exit_code = 1;
        }
        ASSERT(!exception_thrown);
        DeleteFolderDeep(root); // Clean up
    }

    // Test Case 3: Single sandbox root - path outside root
    {
        LOG("Test Case 3: Path outside a single sandbox root.");
        McpServer server(1234, 1);
        String root = NormalizePath(GetExeFolder() + "/test_sandbox_area_2");
        RealizeDirectory(root);
        server.AddSandboxRoot(root);
        LOG("  Added root: " + root);

        String outside_path = NormalizePath(GetExeFolder() + "/some_other_area/file_denied.txt");
        bool exception_thrown = false;
        try {
            server.EnforceSandbox(outside_path);
            LOG("  FAILED: No exception for path outside the root.");
            exit_code = 1;
        } catch (const String& e) {
            LOG("  PASSED: Expected exception thrown: " + e);
            if (e.Find("Sandbox violation") < 0) {
                LOG("  WARNING: Exception message might not be as expected: " + e);
            }
            exception_thrown = true;
        }
        ASSERT(exception_thrown);
        DeleteFolderDeep(root); // Clean up
    }

    // Test Case 4: Multiple sandbox roots
    {
        LOG("Test Case 4: Multiple sandbox roots.");
        McpServer server(1234, 1);
        String root1 = NormalizePath(GetExeFolder() + "/test_sandbox_multi_1");
        String root2 = NormalizePath(GetExeFolder() + "/test_sandbox_multi_2");
        RealizeDirectory(root1); RealizeDirectory(root2);
        server.AddSandboxRoot(root1);
        server.AddSandboxRoot(root2);
        LOG("  Added roots: " + root1 + ", " + root2);

        bool path1_allowed = false;
        bool path2_allowed = false;
        bool path_outside_allowed = true; // Should be false after test

        try {
            server.EnforceSandbox(AppendFileName(root1, "file.txt"));
            path1_allowed = true;
        } catch (...) {}

        try {
            server.EnforceSandbox(AppendFileName(root2, "another.txt"));
            path2_allowed = true;
        } catch (...) {}

        try {
            server.EnforceSandbox(NormalizePath(GetExeFolder() + "/outside_multi/other.txt"));
            // Should throw
        } catch (const String& e) {
            path_outside_allowed = false; // Correctly denied
        }

        if (path1_allowed && path2_allowed && !path_outside_allowed) {
            LOG("  PASSED: Paths within multiple roots allowed, path outside denied.");
        } else {
            LOG("  FAILED: Logic error with multiple roots. path1_ok=" + AsString(path1_allowed) +
                ", path2_ok=" + AsString(path2_allowed) + ", path_outside_denied=" + AsString(!path_outside_allowed));
            exit_code = 1;
        }
        ASSERT(path1_allowed && path2_allowed && !path_outside_allowed);
        DeleteFolderDeep(root1); DeleteFolderDeep(root2);
    }

    // Test Case 5: Path is exactly a sandbox root
    {
        LOG("Test Case 5: Path is exactly a sandbox root.");
        McpServer server(1234, 1);
        String root = NormalizePath(GetExeFolder() + "/test_sandbox_exact");
        RealizeDirectory(root);
        server.AddSandboxRoot(root);
         LOG("  Added root: " + root);

        bool exception_thrown = false;
        try {
            server.EnforceSandbox(root); // Path is the root itself
            LOG("  PASSED: No exception for path being exactly a root.");
        } catch (const String& e) {
            LOG("  FAILED: Exception for path being exactly a root: " + e);
            exception_thrown = true;
            exit_code = 1;
        }
        ASSERT(!exception_thrown);
        DeleteFolderDeep(root);
    }

    // Test Case 6: Child path of a root that contains '..' but still resolves within sandbox
    {
        LOG("Test Case 6: Path with '..' but resolves inside sandbox.");
        McpServer server(1234, 1);
        String root = NormalizePath(GetExeFolder() + "/test_sandbox_parent");
        String childDir = AppendFileName(root, "child");
        RealizeDirectory(childDir);
        server.AddSandboxRoot(root);
        LOG("  Added root: " + root);

        String trickyPath = AppendFileName(childDir, "../file_in_root.txt");
        // This resolves to GetExeFolder() + "/test_sandbox_parent/file_in_root.txt"
        // which is inside the sandbox. NormalizePath handles '..'.

        bool exception_thrown = false;
        try {
            server.EnforceSandbox(trickyPath);
            LOG("  PASSED: No exception for tricky path resolving inside sandbox.");
        } catch (const String& e) {
            LOG("  FAILED: Exception for tricky path: " + e);
            exception_thrown = true;
            exit_code = 1;
        }
        ASSERT(!exception_thrown);
        DeleteFolderDeep(root);
    }

    // Test Case 7: Path with '..' that attempts to escape sandbox
    {
        LOG("Test Case 7: Path with '..' attempting to escape sandbox.");
        McpServer server(1234, 1);
        String root = NormalizePath(GetExeFolder() + "/test_sandbox_escape");
        RealizeDirectory(root);
        server.AddSandboxRoot(root);
        LOG("  Added root: " + root);

        String trickyPath = AppendFileName(root, "../../escaped_file.txt");
        // This resolves to GetExeFolder() + "/../escaped_file.txt" (one level above exe dir)
        // which should be outside the sandbox root.

        bool exception_thrown = false;
        try {
            server.EnforceSandbox(trickyPath);
            LOG("  FAILED: No exception for path escaping sandbox with '..'. Path: " + NormalizePath(trickyPath));
            exit_code = 1;
        } catch (const String& e) {
            LOG("  PASSED: Expected exception for sandbox escape attempt: " + e);
            exception_thrown = true;
        }
        ASSERT(exception_thrown);
        DeleteFolderDeep(root);
    }


    if (exit_code == 0) {
        LOG("--- All Sandbox Tests PASSED ---");
    } else {
        LOG("--- Some Sandbox Tests FAILED ---");
    }
    SetExitCode(exit_code);
}
