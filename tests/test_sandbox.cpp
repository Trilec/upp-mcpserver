#include "../include/McpServer.h"
#include <Core/Core.h>
#include "test_helpers.h"

TEST(Sandbox_NoRootsAllowsAnyPath)
{
    McpServer server(1234, 1);
    server.EnforceSandbox("/some/random/path/file.txt");
    server.EnforceSandbox("C:/windows/system32/dangerous.dll");
}

TEST(Sandbox_PathWithinRoot)
{
    McpServer server(1234, 1);
    String root = NormalizePath(GetExeFolder() + "/test_sandbox_area_1");
    RealizeDirectory(root);
    server.AddSandboxRoot(root);
    server.EnforceSandbox(AppendFileName(root, "file_allowed.txt"));
    DeleteFolderDeep(root);
}

TEST(Sandbox_PathOutsideRoot)
{
    McpServer server(1234, 1);
    String root = NormalizePath(GetExeFolder() + "/test_sandbox_area_2");
    RealizeDirectory(root);
    server.AddSandboxRoot(root);
    String outside = NormalizePath(GetExeFolder() + "/some_other_area/file_denied.txt");
    bool threw = false;
    try {
        server.EnforceSandbox(outside);
    } catch(...) {
        threw = true;
    }
    ASSERT(threw);
    DeleteFolderDeep(root);
}

TEST(Sandbox_MultipleRoots)
{
    McpServer server(1234, 1);
    String root1 = NormalizePath(GetExeFolder() + "/test_sandbox_multi_1");
    String root2 = NormalizePath(GetExeFolder() + "/test_sandbox_multi_2");
    RealizeDirectory(root1); RealizeDirectory(root2);
    server.AddSandboxRoot(root1);
    server.AddSandboxRoot(root2);
    server.EnforceSandbox(AppendFileName(root1, "file.txt"));
    server.EnforceSandbox(AppendFileName(root2, "another.txt"));
    bool threw = false;
    try {
        server.EnforceSandbox(NormalizePath(GetExeFolder() + "/outside_multi/other.txt"));
    } catch(...) {
        threw = true;
    }
    ASSERT(threw);
    DeleteFolderDeep(root1); DeleteFolderDeep(root2);
}

TEST(Sandbox_PathIsRoot)
{
    McpServer server(1234, 1);
    String root = NormalizePath(GetExeFolder() + "/test_sandbox_exact");
    RealizeDirectory(root);
    server.AddSandboxRoot(root);
    server.EnforceSandbox(root);
    DeleteFolderDeep(root);
}

TEST(Sandbox_TrickyPathInside)
{
    McpServer server(1234, 1);
    String root = NormalizePath(GetExeFolder() + "/test_sandbox_parent");
    String child = AppendFileName(root, "child");
    RealizeDirectory(child);
    server.AddSandboxRoot(root);
    String tricky = AppendFileName(child, "../file_in_root.txt");
    server.EnforceSandbox(tricky);
    DeleteFolderDeep(root);
}

TEST(Sandbox_TrickyPathEscape)
{
    McpServer server(1234, 1);
    String root = NormalizePath(GetExeFolder() + "/test_sandbox_escape");
    RealizeDirectory(root);
    server.AddSandboxRoot(root);
    String tricky = AppendFileName(root, "../../escaped_file.txt");
    bool threw = false;
    try {
        server.EnforceSandbox(tricky);
    } catch(...) {
        threw = true;
    }
    ASSERT(threw);
    DeleteFolderDeep(root);
}
