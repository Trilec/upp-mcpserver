cmake_minimum_required(VERSION 3.10)
project(ums-readfile-plugin) // Updated project name

add_executable(ums-readfile-main file_reader_main.cpp) // Target name can also be updated

# Link against mcp_server_lib and U++ Core, Json, WebSockets.
# Placeholder:
# target_link_libraries(ums-readfile-main PRIVATE mcp_server_lib UPP::Core UPP::Json UPP::WebSockets)
