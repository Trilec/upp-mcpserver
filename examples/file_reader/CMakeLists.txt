cmake_minimum_required(VERSION 3.10)
project(FileReaderExample)

# Find U++ package (this is highly dependent on U++ setup)
# This is a placeholder. A real U++ CMake setup would be more complex,
# likely using a FindUpp.cmake module or U++'s own build system (umk).
# For simplicity, we'll assume McpServer library and U++ Core are linkable.

add_executable(FileReaderMain file_reader_main.cpp)

# Link against mcp_server_lib (static library from our project)
# and U++ Core, Json, WebSockets.
# Target link libraries command depends on how these are defined/found.
# Placeholder:
# target_link_libraries(FileReaderMain PRIVATE mcp_server_lib UPP::Core UPP::Json UPP::WebSockets)
