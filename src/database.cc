#include "database.h"

std::optional<RespValue> Database::get(const std::string key) const {
  const auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<RespValue> Database::set(const std::string key, RespValue value) {
  auto it = map.find(key);
  std::optional<RespValue> old_value = std::nullopt;
  if (it != map.end()) {
    old_value = it->second;
    it->second = value;
  } else {
    map.insert({key, value});
  }
  return old_value;
}