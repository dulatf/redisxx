#pragma once
#include <string>
#include <variant>
#include <vector>

#include "resp_types.h"

std::optional<RespValue> parse_resp_code(std::string input);