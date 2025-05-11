#pragma once
#include <optional>
#include <vector>

std::optional<size_t> try_parse(const std::vector<uint8_t> &buffer);