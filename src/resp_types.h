#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct RespValue;
namespace std {
template <>
struct hash<RespValue>;
}

using RespString = std::string;
using RespInteger = long;
using RespArray = std::vector<RespValue>;
struct RespError {
  std::string message;
};
using RespMap = std::unordered_map<RespValue, RespValue>;

template <typename... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

struct RespValue {
  std::variant<RespString, RespInteger, RespArray, RespError, RespMap> value;
  RespValue() = default;  // Default constructor
                          // Explicitly define copy operations
  RespValue(const RespValue& other) = default;
  RespValue& operator=(const RespValue& other) = default;

  // Explicitly define move operations
  RespValue(RespValue&& other) noexcept = default;
  RespValue& operator=(RespValue&& other) noexcept = default;

  // Destructor
  ~RespValue() = default;

  RespValue(const RespString& s) : value(s) {}
  RespValue(RespString&& s) : value(std::move(s)) {}
  RespValue(RespInteger i) : value(i) {}
  RespValue(const RespArray& a) : value(a) {}
  RespValue(RespArray&& a) : value(std::move(a)) {}
  RespValue(const RespError& e) : value(e) {}
  RespValue(RespError&& e) : value(std::move(e)) {}
  RespValue(const RespMap& m) : value(m) {}
  RespValue(RespMap&& m) : value(std::move(m)) {}

  static RespValue make_string(std::string s) {
    return RespValue(std::move(s));
  }
  static RespValue make_integer(u_long i) { return RespValue(i); }
  static RespValue make_array(RespArray a) { return RespValue(std::move(a)); }
  static RespValue make_error(std::string message) {
    return RespValue(RespError{.message = std::move(message)});
  }
  static RespValue make_map(std::unordered_map<RespValue, RespValue> map) {
    return RespValue(std::move(map));
  }

  bool operator==(const RespValue& other) const { return value == other.value; }

  std::string to_string() const {
    auto display_fn = Overload{
        [](RespString str) -> std::string { return str; },
        [](RespInteger num) -> std::string { return std::to_string(num); },
        [](RespArray arr) -> std::string {
          std::ostringstream oss;
          oss << "[";
          for (const auto& el : arr) {
            oss << el.to_string() << ", ";
          }
          oss << "]";
          return oss.str();
        },
        [](RespError err) -> std::string {
          std::ostringstream oss;
          oss << "Error: " << err.message;
          return oss.str();
        },
        [](RespMap map) -> std::string {
          std::ostringstream oss;
          oss << "{";
          for (const auto& [k, v] : map) {
            oss << k.to_string() << ": " << v.to_string() << ", ";
          }
          oss << "}";
          return oss.str();
        }};
    return std::visit(display_fn, this->value);
  }

  RespArray to_array_safe() const {
    const auto visitor = Overload{
        [](RespArray arr) { return arr; },
        [](RespString str) { return RespArray(std::vector<RespValue>{str}); },
        [](RespInteger num) { return RespArray(std::vector<RespValue>{num}); },
        [](RespError err) { return RespArray(std::vector<RespValue>{err}); },
        [](RespMap map) {
          std::vector<RespValue> arr;
          for (const auto& [k, v] : map) {
            arr.push_back(RespValue::make_array({k, v}));
          }
          return RespArray(arr);
        }};
    return std::visit(visitor, this->value);
  }
  std::string to_protocol_representation() const {
    const auto visitor = Overload{
        [](RespArray arr) {
          std::ostringstream oss;
          oss << "*" << arr.size() << "\r\n";
          for (const auto& el : arr) {
            oss << el.to_protocol_representation();
          }
          return oss.str();
        },
        [](RespString str) {
          std::ostringstream oss;
          oss << "$" << str.length() << "\r\n" << str << "\r\n";
          return oss.str();
        },
        [](RespInteger num) {
          std::ostringstream oss;
          oss << ":" << num << "\r\n";
          return oss.str();
        },
        [](RespError err) {
          std::ostringstream oss;
          oss << "-" << err.message << "\r\n";
          return oss.str();
        },
        [](RespMap map) {
          std::ostringstream oss;
          oss << "%" << map.size() << "\r\n";
          for (const auto& [k, v] : map) {
            oss << k.to_protocol_representation()
                << v.to_protocol_representation();
          }
          return oss.str();
        },
    };
    return std::visit(visitor, this->value);
  }
};

namespace std {
template <>
struct hash<RespValue> {
  size_t operator()(const RespValue& val) const {
    return std::hash<std::string>()(val.to_protocol_representation());
  }
};
}  // namespace std
