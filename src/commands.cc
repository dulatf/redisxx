#include "commands.h"

#include <algorithm>
#include <iostream>

std::string to_lower(const std::string &s) {
  std::string out;
  std::transform(s.begin(), s.end(), std::back_inserter(out),
                 [](const auto c) { return std::tolower(c); });
  return out;
}

std::optional<RespValue> dispatch_commands(RespString command,
                                           const RespArray &arguments) {
  std::cout << "Attempting to handle command `" << command << "`\n";
  const auto result =
      CommandRegistry::instance().execute_command(to_lower(command), arguments);
  if (!result) {
    std::cout << "Failed to handle command `" << command << "`\n";
  }
  return result;
}

RespValue handle_ping(const RespArray &arguments) {
  if (arguments.size() > 1) {
    return RespValue::make_error(
        "ERR Wrong number of arguments for PING command.");
  }
  if (arguments.empty()) {
    return RespValue::make_string("PONG");
  }
  return RespValue::make_string(arguments.front().to_string());
}
CommandRegistrar _handle_ping("ping", handle_ping);

RespValue handle_command(const RespArray &arguments) {
  if (arguments.size() != 1) {
    return RespValue::make_error(
        "ERR Wrong number of arguments for COMMAND command");
  }
  if (to_lower(arguments.front().to_string()) != "docs") {
    return RespValue::make_error("ERR Currently only supporting COMMAND DOCS");
  }
  std::unordered_map<RespString, RespValue> map;
  for (const auto &command : CommandRegistry::instance().list_commands()) {
    map[command] = RespValue::make_array({RespValue::make_string(command)});
  }
  return RespValue::make_map(map);
}
CommandRegistrar _handle_command("command", handle_command);
