#pragma once
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>

#include "resp_types.h"

class CommandRegistry {
 public:
  static CommandRegistry& instance() {
    static CommandRegistry instance;
    return instance;
  }
  template <std::invocable<const RespArray&> Func>
  void register_command(std::string name, Func&& func) {
    commands[std::string(name)] = std::forward<Func>(func);
  }

  std::optional<RespValue> execute_command(std::string name,
                                           const RespArray& arguments) const {
    auto it = commands.find(name);
    if (it == commands.end()) {
      return std::nullopt;
    }
    return it->second(arguments);
  }

  std::vector<std::string> list_commands() {
    std::vector<std::string> result;
    result.reserve(commands.size());
    for (const auto& [name, _] : commands) {
      result.push_back(name);
    }
    return result;
  }

 private:
  CommandRegistry() = default;
  // Delete copy/move operations
  CommandRegistry(const CommandRegistry&) = delete;
  CommandRegistry& operator=(const CommandRegistry&) = delete;
  CommandRegistry(CommandRegistry&&) = delete;
  CommandRegistry& operator=(CommandRegistry&&) = delete;

  std::unordered_map<std::string, std::function<RespValue(const RespArray&)>>
      commands;
};

struct CommandRegistrar {
 public:
  template <std::invocable<const RespArray&> Func>
  CommandRegistrar(std::string name, Func&& func) {
    CommandRegistry::instance().register_command(name,
                                                 std::forward<Func>(func));
  }
};

std::string to_lower(const std::string& s);

std::optional<RespValue> dispatch_commands(RespString command,
                                           const RespArray& arguments);
