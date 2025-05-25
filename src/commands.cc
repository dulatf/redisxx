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

bool check_for_option(const RespArray &arguments, RespString option) {
  for (const auto &arg : arguments) {
    if (std::holds_alternative<RespString>(arg.value)) {
      if (to_lower(std::get<RespString>(arg.value)) == option) {
        return true;
      }
    }
  }
  return false;
}

std::optional<RespValue> extract_option_value(const RespArray &arguments,
                                              RespString option) {
  for (auto it = arguments.begin(); it != arguments.end(); ++it) {
    std::cout << "[extract_option_value(" << option << ")] " << it->to_string()
              << std::endl;
    if (std::holds_alternative<RespString>(it->value)) {
      if (to_lower(std::get<RespString>(it->value)) == option) {
        std::cout << "Found a match on " << option << std::endl;
        const auto next = it + 1;
        std::cout << "Returning " << next->to_string() << std::endl;
        if (next != arguments.end()) {
          return *next;
        }
      }
    }
  }
  return std::nullopt;
}

std::optional<RespValue> dispatch_commands(RespString command,
                                           const RespArray &arguments) {
  std::cout << "Attempting to handle command `" << command
            << "` with arguments\n";
  for (const auto &arg : arguments) {
    std::cout << "- " << arg.debug_format_type() << " :: " << arg.to_string()
              << "\n";
  }
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
  if (check_for_option(arguments, "px") && check_for_option(arguments, "ex")) {
    return RespValue::make_error("ERR Only one of EX or PX is allowed");
  }
  const auto expire_in =
      extract_option_value(arguments, RespString("px"))
          .and_then(
              [](RespValue val) -> std::optional<std::chrono::milliseconds> {
                std::cout << "Processing " << val.to_string() << std::endl;
                if (std::holds_alternative<RespInteger>(val.value)) {
                  std::cout << "Got integer" << std::endl;
                  return std::chrono::milliseconds(
                      std::get<RespInteger>(val.value));
                } else if (std::holds_alternative<RespString>(val.value)) {
                  std::cout << "That was unexpected: " << val.to_string();
                  return std::chrono::milliseconds(
                      std::stoi(std::get<RespString>(val.value)));
                } else {
                  std::cout << "Got nothing" << std::endl;
                  return std::nullopt;
                }
              })
          .or_else([&arguments]() {
            return extract_option_value(arguments, RespString("ex"))
                .and_then([](RespValue val)
                              -> std::optional<std::chrono::milliseconds> {
                  if (std::holds_alternative<RespInteger>(val.value)) {
                    return std::chrono::seconds(
                        std::get<RespInteger>(val.value));
                  } else {
                    return std::nullopt;
                  }
                });
          });
  const auto old_value = Database::instance().set(arguments[0].to_string(),
                                                  arguments[1], expire_in);
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

RespValue handle_client(const RespArray &arguments) {
  return RespValue::make_string("OK");
}
CommandRegistrar _handle_client("client", handle_client);

RespValue handle_hello(const RespArray &arguments) {
  if (arguments.size() < 1) {
    return RespValue::make_error("ERR too few arguments for HELLO");
  }
  const auto protocol_version = arguments[0].to_int_safe();
  if (!protocol_version || *protocol_version != 3) {
    return RespValue::make_error("ERR invalid protocol version");
  }
  return RespValue::make_map(
      RespMap({{RespString("proto"), RespValue::make_integer(3)}}));
}
CommandRegistrar _handle_hello("hello", handle_hello);

RespValue inner_incrby(RespString key, RespInteger increment) {
  const auto value = Database::instance().get(key);
  if (!value) {
    const long new_value = increment;
    Database::instance().set(key, RespValue::make_integer(new_value),
                             std::nullopt);
    return new_value;
  } else {
    const auto int_value = value->to_int_safe();
    if (!int_value) {
      return RespValue::make_error("ERR value is not an integer");
    }
    const long new_value = (*int_value) + increment;
    Database::instance().set(key, RespValue::make_integer(new_value),
                             std::nullopt);
    return new_value;
  }
}

RespValue handle_incr(const RespArray &arguments) {
  if (arguments.size() != 1) {
    return RespValue::make_error("ERR wrong number of arguments for INCR");
  }
  const auto key = arguments[0].to_string();
  return inner_incrby(key, RespInteger(1));
}
CommandRegistrar _handle_incr("incr", handle_incr);

RespValue handle_incrby(const RespArray &arguments) {
  if (arguments.size() != 2) {
    return RespValue::make_error("ERR wrong number of arguments for INCRBY");
  }
  const auto key = arguments[0].to_string();
  const auto increment = arguments[1].to_int_safe();
  if (!increment) {
    return RespValue::make_error("ERR invalid increment");
  }
  return inner_incrby(key, *increment);
}
CommandRegistrar _handle_incrby("incrby", handle_incrby);

RespValue handle_decr(const RespArray &arguments) {
  if (arguments.size() != 1) {
    return RespValue::make_error("ERR wrong number of arguments for DECR");
  }
  const auto key = arguments[0].to_string();
  return inner_incrby(key, RespInteger(-1));
}
CommandRegistrar _handle_decr("decr", handle_decr);

RespValue handle_decrby(const RespArray &arguments) {
  if (arguments.size() != 2) {
    return RespValue::make_error("ERR wrong number of arguments for DECRBY");
  }
  const auto key = arguments[0].to_string();
  const auto increment = arguments[1].to_int_safe();
  if (!increment) {
    return RespValue::make_error("ERR invalid increment");
  }
  return inner_incrby(key, -(*increment));
}
CommandRegistrar _handle_decrby("decrby", handle_decrby);

RespValue handle_config(const RespArray &arguments) {
  if (arguments.size() != 2) {
    return RespValue::make_error("ERR wrong number of arguments for CONFIG");
  }
  if (to_lower(arguments[0].to_string()) != "get") {
    return RespValue::make_error("ERR unsupported sub command for CONFIG: " +
                                 arguments[0].to_string());
  }
  static std::unordered_map<RespString, RespString> config_map(
      {{"save", ""}, {"appendonly", "no"}});
  auto config_val = config_map.find(arguments[1].to_string());
  if (config_val == config_map.end()) {
    std::cout << "Unknown config key: `" << arguments[1].to_string() << "`\n";
    return RespValue::make_string("");
  }
  return RespValue::make_string(config_val->second);
}
CommandRegistrar _handle_config("config", handle_config);