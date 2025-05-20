#include "commands.h"

#include <algorithm>
#include <iostream>

#include "database.h"

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
    return RespValue::make_error("ERR Wrong number of arguments for PING.");
  }
  if (arguments.empty()) {
    return RespValue::make_string("PONG");
  }
  return RespValue::make_string(arguments.front().to_string());
}
CommandRegistrar _handle_ping("ping", handle_ping);

RespValue handle_command(const RespArray &arguments) {
  if (arguments.size() != 1) {
    return RespValue::make_error("ERR Wrong number of arguments for COMMAND.");
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

RespValue handle_echo(const RespArray &arguments) {
  if (arguments.size() != 1) {
    return RespValue::make_error("ERR Wrong number of arguments for ECHO.");
  }
  return RespValue::make_string(arguments.front().to_string());
}
CommandRegistrar _handle_echo("echo", handle_ping);

RespValue handle_set(const RespArray &arguments) {
  if (arguments.size() < 2) {
    return RespValue::make_error("ERR Too few arguments for SET.");
  }
  const auto old_value =
      Database::instance().set(arguments[0].to_string(), arguments[1]);
  return RespValue::make_string("OK");
}
CommandRegistrar _handle_set("set", handle_set);

RespValue handle_get(const RespArray &arguments) {
  if (arguments.size() < 1) {
    return RespValue::make_error("ERR Too few arguments for SET.");
  }
  const auto value = Database::instance().get(arguments[0].to_string());
  if (!value) {
    return RespValue::make_null();
  }
  return RespValue::make_string(value->to_string());
}
CommandRegistrar _handle_get("get", handle_get);
