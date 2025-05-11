#pragma once
#include <arpa/inet.h>

#include <string>

std::string ipv4_to_string(const struct in_addr &in, long port);
std::string ipv6_to_string(const struct in6_addr &in, long port);
