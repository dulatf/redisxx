#include "database.h"

#include <iostream>

std::optional<RespValue> Database::get(const std::string key) {
  expire_keys();
  const auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<RespValue> Database::set(
    const std::string key, RespValue value,
    std::optional<std::chrono::milliseconds> expire_in) {
  expire_keys();
  if (expire_in) {
    std::cout << "Marking key " << key << " to expire in " << *expire_in
              << "ms." << std::endl;
    const auto now = std::chrono::steady_clock::now();
    const auto expire_on = now + *expire_in;
    expiring_keys.insert({key, expire_on});
  }
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

void Database::expire_keys() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = expiring_keys.begin(); it != expiring_keys.end(); /* */) {
    if (it->second <= now) {  // Key is expiring
      map.erase(it->first);
      it = expiring_keys.erase(it);
    } else {
      ++it;
    }
  }
}