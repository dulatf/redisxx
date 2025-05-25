#pragma once
#include <chrono>
#include <optional>
#include <unordered_map>

#include "resp_types.h"

class Database {
 public:
  static Database& instance() {
    static Database instance;
    return instance;
  }

  std::optional<RespValue> get(std::string key);
  std::optional<RespValue> set(
      std::string key, RespValue value,
      std::optional<std::chrono::milliseconds> expire_in);

  void expire_keys();

 private:
  Database() = default;
  // Delete copy/move operations
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(Database&&) = delete;

  std::unordered_map<std::string, RespValue> map;
  std::unordered_map<std::string,
                     std::chrono::time_point<std::chrono::steady_clock>>
      expiring_keys;
};