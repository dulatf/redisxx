#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <variant>
#include <vector>

struct RespValue;
using RespString = std::string;
using RespInteger = long;
using RespArray = std::vector<RespValue>;

template <typename... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

struct RespValue {
  std::variant<RespString, RespInteger, RespArray> value;
  RespValue() = default;  // Default constructor
  RespValue(const RespString& s) : value(s) {}
  RespValue(RespString&& s) : value(std::move(s)) {}
  RespValue(RespInteger i) : value(i) {}
  RespValue(const RespArray& a) : value(a) {}
  RespValue(RespArray&& a) : value(std::move(a)) {}

  static RespValue make_string(std::string s) {
    return RespValue(std::move(s));
  }
  static RespValue make_integer(u_long i) { return RespValue(i); }
  static RespValue make_array(RespArray a) { return RespValue(std::move(a)); }

  std::string to_string() const {
    auto display_fn = Overload{
        [](RespString str) -> std::string { return str; },
        [](RespInteger num) -> std::string { return std::to_string(num); },
        [](RespArray arr) -> std::string {
          std::ostringstream oss;
          oss << "[";
          for (const auto& el : arr) {
            oss << el.to_string() << " ";
          }
          oss << "]";
          return oss.str();
        },
    };
    return std::visit(display_fn, this->value);
  }
};
