#include "../../include/McpServer.h"
#include <Core/Core.h>
#include <Core/Json.h> // For JsonObject, JsonArray, Value
#include <Core/ValueUtil.h> // For AsJson for logging (if used)

JsonValue CalculateToolLogic(McpServer& server, const JsonObject& args){ // Added server ref for consistency, though not used
    Value va=args.Get("a"),vb=args.Get("b"); String op=args.Get("operation","").ToString();
    if(!va.IsNumber())throw Exc("Argument error: 'a' must be a number for 'ums-calc' tool.");
    if(!vb.IsNumber())throw Exc("Argument error: 'b' must be a number for 'ums-calc' tool.");
    if(op.IsEmpty())throw Exc("Argument error: 'operation' is required for 'ums-calc' tool.");
    double a=va.To<double>(),b=vb.To<double>();
    if(op=="add")return a+b; if(op=="subtract")return a-b; if(op=="multiply")return a*b;
    if(op=="divide"){if(b==0)throw Exc("Arithmetic error: Division by zero in 'ums-calc' tool."); return a/b;}
    throw Exc("Argument error: Unknown operation '" + op + "' for 'ums-calc' tool. Supported: add, subtract, multiply, divide.");
}

CONSOLE_APP_MAIN {
    StdLogSetup(LOG_COUT|LOG_TIMESTAMP); SetExitCode(0);
    LOG("--- UMS Calc Example ---");
    McpServer server(5002,10);
    server.SetLogCallback([](const String&m){LOG("[S]: "+m);});

    ToolDefinition td;
    td.description="ums-calc: Perform add, subtract, multiply, or divide on two numbers. No special permissions required.";
    td.parameters("a",JsonObject("type","number")("description","First operand (number)."))
                 ("b",JsonObject("type","number")("description","Second operand (number)."))
                 ("operation",JsonObject("type","string")("description","Operation to perform: 'add', 'subtract', 'multiply', 'divide'."));
    // Pass server instance to the lambda for consistency, even if not used by this specific tool logic
    td.func = [&](const JsonObject& args) -> Value { return CalculateToolLogic(server, args); };

    const String toolName = "ums-calc"; // Updated tool name
    server.AddTool(toolName,td);
    server.EnableTool(toolName);
    LOG("Tool '"+toolName+"' added and enabled.");

    server.ConfigureBind(true);
    if(server.StartServer()){
        LOG("Server started on port 5002. Call tool '"+toolName+"'.");
        LOG("Example tool call: { \"type\": \"tool_call\", \"tool\": \"" + toolName + "\", \"args\": { \"a\": 20, \"b\": 5, \"operation\": \"subtract\" } }");
        while(true)Sleep(1000);
    } else {
        LOG("Server start failed.");
        SetExitCode(1);
    }
}
