#include "commands.h"

#include <iostream>
std::optional<RespValue> handle_command(RespString command,
                                        const RespArray &arguments) {
  std::cout << "Attempting to handle command `" << command << "`\n";
  const auto result =
      CommandRegistry::instance().execute_command(command, arguments);
  if (!result) {
    std::cout << "Failed to handle command `" << command << "`\n";
  }
  return result;
}

RespValue handle_ping(const RespArray &arguments) {
  std::cout << "Handling PING" << std::endl;
  if (arguments.size() > 1) {
    return RespValue::make_error(
        "ERR Wrong number of arguments for PING command.");
  }
  if (arguments.empty()) {
    return RespValue::make_string("PONG");
  }
  return RespValue::make_string(arguments.front().to_string());
}
CommandRegistrar _handle_ping("PING", handle_ping);