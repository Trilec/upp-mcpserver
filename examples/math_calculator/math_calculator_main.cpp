/**
 * @file math_calculator_main.cpp
 * @brief Example plugin: calculate tool.
 *
 * This console application demonstrates how to register and run
 * the "calculate" tool with an McpServer instance.
 */

#include "../../include/McpServer.h" // Path to McpServer.h
#include <Core/Core.h>             // For U++ Core utilities
#include <Json/Json.h>             // For JsonObject, JsonValue

// Tool logic for calculate
JsonValue CalculateToolLogic(McpServer& /*server*/, const JsonObject& args) {
    // This tool does not require special permissions or sandbox.
    double a = args.Get("a", 0.0);
    if (args.Get("a").IsVoid()) throw String("Argument error: 'a' is required.");

    double b = args.Get("b", 0.0);
    if (args.Get("b").IsVoid()) throw String("Argument error: 'b' is required.");

    String op = args.Get("operation", "");
    if (op.IsEmpty()) throw String("Argument error: 'operation' is required.");

    if (op == "add") return a + b;
    if (op == "subtract") return a - b;
    if (op == "multiply") return a * b;
    if (op == "divide") {
        if (b == 0) throw String("Arithmetic error: Division by zero.");
        return a / b;
    }
    throw String("Argument error: Unknown operation '") + op + "'. Supported operations: add, subtract, multiply, divide.";
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP);
    SetExitCode(0);

    LOG("--- Math Calculator Example Plugin ---");

    // 1. Instantiate McpServer
    int port = 5002; // Different port for standalone testing
    McpServer server(port, 10);
    LOG("McpServer instance created for math_calculator_example on port " + AsString(port));

    server.SetLogCallback([](const String& msg) {
        LOG("[Server]: " + msg);
    });

    // 2. Configure server instance (no special permissions or sandbox needed for calculate)
    LOG("No special permissions or sandbox roots required for the 'calculate' tool.");

    // 3. Define ToolDefinition
    ToolDefinition td;
    td.description = "Perform add, subtract, multiply, or divide on two numbers. No special permissions required.";

    JsonObject params;
    params("a", JsonObject("type", "number")("description", "First operand (number)."));
    params("b", JsonObject("type", "number")("description", "Second operand (number)."));
    params("operation", JsonObject("type", "string")("description", "Operation to perform: 'add', 'subtract', 'multiply', 'divide'."));
    td.parameters = params;

    td.func = [&](const JsonObject& args) -> JsonValue {
        return CalculateToolLogic(server, args);
    };

    // 4. Add and Enable Tool
    const String toolName = "calculate_example";
    server.AddTool(toolName, td);
    server.EnableTool(toolName);
    LOG("Tool '" + toolName + "' added and enabled.");

    // 5. Start Server
    server.ConfigureBind(true); // Bind to all interfaces
    if (server.StartServer()) {
        LOG("Server started. Connect a WebSocket client to ws://localhost:" + AsString(port));
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"a\": 10, \"b\": 5, \"operation\": \"add\" } }");
        while (true) {
            Sleep(1000);
        }
    } else {
        LOG("Failed to start the server on port " + AsString(port));
        SetExitCode(1);
    }
}
