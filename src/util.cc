#include "util.h"

#include <sstream>

std::string ipv6_to_string(const struct in6_addr &in, long port) {
  char ip_str_buf[INET6_ADDRSTRLEN];
  const char *result =
      inet_ntop(AF_INET6,         // Address Family: IPv6
                &in,              // Pointer to the binary address
                ip_str_buf,       // Pointer to the destination buffer
                INET6_ADDRSTRLEN  // Size of the destination buffer
      );

  if (result == nullptr) {
    throw std::runtime_error("Failed to convert ipv6 address: " +
                             std::string(strerror(errno)));
  }
  std::ostringstream oss;
  oss << "[" << std::string(ip_str_buf) << "] : " << port;
  return oss.str();
}
std::string ipv4_to_string(const struct in_addr &in, long port) {
  char ip_str_buf[INET_ADDRSTRLEN];
  const char *result = inet_ntop(AF_INET, &in, ip_str_buf, INET_ADDRSTRLEN);
  if (result == nullptr) {
    throw std::runtime_error("Failed to convert ipv4 address: " +
                             std::string(strerror(errno)));
  }
  std::ostringstream oss;
  oss << std::string(ip_str_buf) << " : " << port;
  return oss.str();
}