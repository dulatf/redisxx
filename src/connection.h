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

EventState handle_read(Connection &con) {
  std::cout << "Handling `read` on socket " << con.fd << std::endl;
  uint8_t buffer[1024] = {0};
  const size_t bytes_read = recv(con.fd, buffer, sizeof(buffer), 0);
  std::cout << "Received " << bytes_read << " bytes:\n"
            << (const char *)buffer << "\n";
  con.incoming.insert(con.incoming.end(), &buffer[0], &buffer[0] + bytes_read);
  if (con.incoming.size() < 10) {
    return EventState::Read;
  }
  con.outgoing.insert(con.outgoing.begin(), con.incoming.begin(),
                      con.incoming.end());
  return EventState::Write;
}

EventState handle_write(Connection &con) {
  std::cout << "Handling `write` on socket " << con.fd << std::endl;
  const auto bytes_written =
      send(con.fd, con.outgoing.data(), con.outgoing.size(), 0);
  std::cout << "Wrote " << bytes_written << " bytes." << std::endl;
  con.outgoing.erase(con.outgoing.begin(),
                     con.outgoing.begin() + bytes_written);
  if (!con.outgoing.empty()) {
    return EventState::Write;
  }
  return EventState::Idle;
}