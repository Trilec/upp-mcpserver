cmake_minimum_required(VERSION 3.10)
project(McpServerTests)

add_executable(McpServerTests
    test_main.cpp
    test_sandbox.cpp
    test_permissions.cpp
)

target_include_directories(McpServerTests PRIVATE ${CMAKE_SOURCE_DIR}/include)
# Link against core server library when available
# target_link_libraries(McpServerTests PRIVATE mcp_server_lib)
