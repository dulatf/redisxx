#include "protocol.h"

typedef std::vector<uint8_t>::iterator It;

bool parse_terminal(It cur, It end, uint8_t terminal) {
  if (cur == end) {
    return false;
  }
  if (*cur == terminal) {
    cur++;
    return true;
  }
  return false;
}

std::optional<int> parse_digit(It cur, It end) {
  if (cur == end) {
    return false;
  }
  if (std::isdigit(*cur)) {
    const int val = *cur - '0';
    return val;
  }
  return std::nullopt;
}

std::optional<size_t> try_parse(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return std::nullopt;
  }
}