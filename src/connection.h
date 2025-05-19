#pragma once
#include <sys/socket.h>

#include <iostream>
#include <vector>

struct Connection {
  int fd = -1;
  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;

 public:
  Connection(int handle) : fd{handle}, incoming{}, outgoing{} {}
};

enum class EventState { Idle, Read, Write, Close };

EventState handle_read(Connection &con);
EventState handle_write(Connection &con);