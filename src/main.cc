#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <optional>
#include <strstream>
#include <unordered_map>

#include "connection.h"
#include "util.h"

bool set_nonblocking(int fd) {
  extern int errno;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    std::cerr << "Failed to get socket flags: " << errno << std::endl;
    return false;
  }
  std::cout << "Socket flags 0x" << std::hex << flags << std::dec << std::endl;
  flags |= O_NONBLOCK;
  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    std::cerr << "Failed to set socket flags " << flags << std::endl;
    return false;
  }
  return true;
}

std::optional<int> create_socket(long port) {
  extern int errno;
  const int fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "Failed to open socket." << std::endl;
    return std::nullopt;
  }
  if (!set_nonblocking(fd)) {
    std::cerr << "Failed to make socket nonblocking." << std::endl;
    return std::nullopt;
  }
  const int value = 1;
  errno = 0;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
    std::cerr << "Failed to set socket to reuse mode: " << errno << std::endl;
    return std::nullopt;
  }

  sockaddr_in6 addr = {};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port);
  addr.sin6_addr = in6addr_any;
  int rv = bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
  if (rv) {
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    return std::nullopt;
  }
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    std::cerr << "Failed to listen on socket: " << errno << std::endl;
    return std::nullopt;
  }
  std::cout << "Listening on " << ipv6_to_string(addr.sin6_addr, port)
            << std::endl;
  return fd;
}

constexpr int event_batch_size = 32;

int main(int, char **) {
  std::cout << "ReDiSxx" << std::endl;
  auto socket = create_socket(1234);
  if (!socket) {
    std::cerr << "Failed to open socket." << std::endl;
    return -1;
  }
  std::cout << "Opened socket " << *socket << std::endl;
  const int kq_fd = kqueue();

  struct kevent evSet;
  EV_SET(&evSet, *socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  assert(-1 != kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr));
  std::unordered_map<int, Connection> connection_map{};
  while (1) {
    struct kevent events[event_batch_size];
    const int num_events =
        kevent(kq_fd, nullptr, 0, events, event_batch_size, nullptr);
    std::cout << "Got " << num_events << " events." << std::endl;
    for (int i = 0; i < num_events; ++i) {
      const int in_fd = static_cast<int>(events[i].ident);
      std::cout << "Event " << i << " flags 0x" << std::hex << events[i].flags
                << std::dec << " ident " << in_fd << std::endl;
      // New connection
      if (events[i].flags & EV_ADD && in_fd == *socket) {
        struct sockaddr_storage addr;
        socklen_t socklen = sizeof(addr);
        const int conn_fd =
            accept(in_fd, reinterpret_cast<struct sockaddr *>(&addr), &socklen);
        assert(conn_fd != -1);

        EV_SET(&evSet, conn_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr);
        if (addr.ss_family == AF_INET) {
          const struct sockaddr_in *addr_in = (const struct sockaddr_in *)&addr;
          std::cout << "Connection established from "
                    << ipv4_to_string(addr_in->sin_addr,
                                      ntohl(addr_in->sin_port))
                    << " -> " << conn_fd << std::endl;
        } else {
          const struct sockaddr_in6 *addr_in6 =
              (const struct sockaddr_in6 *)&addr;
          std::cout << "Connection established from "
                    << ipv6_to_string(addr_in6->sin6_addr,
                                      ntohl(addr_in6->sin6_port))
                    << " -> " << conn_fd << std::endl;
        }
        assert(set_nonblocking(conn_fd));
        connection_map.insert({conn_fd, Connection(conn_fd)});
        std::cout << "Registered connection conn_fd=" << conn_fd
                  << " in_fd=" << in_fd << std::endl;
      } else if (events[i].flags & EV_EOF) {  // Disconnect
        std::cout << "Disconnected " << in_fd << std::endl;
        close(in_fd);
        connection_map.erase(in_fd);
      } else if (events[i].filter == EVFILT_READ) {  // Incoming data
        const auto state = handle_read(connection_map.at(in_fd));
        if (state == EventState::Write) {
          EV_SET(&evSet, in_fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
                 nullptr);
          kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr);
        }
      } else if (events[i].filter == EVFILT_WRITE) {  // Write outgoing
        const auto state = handle_write(connection_map.at(in_fd));
        if (state == EventState::Write) {
          EV_SET(&evSet, in_fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
                 nullptr);
          kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr);
        }
      }
    }
  }
  return 0;
}