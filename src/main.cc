#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <optional>

bool set_nonblocking(int fd) {
  extern int errno;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    std::cerr << "Failed to get socket flags: " << errno << std::endl;
    return false;
  }
  std::cout << "Socket flags " << flags << std::endl;
  flags |= O_NONBLOCK;
  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno) {
    std::cerr << "Failed to set socket flags " << flags << std::endl;
    return false;
  }
  return true;
}

std::optional<int> create_socket() {
  extern int errno;
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
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

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0);
  int rv = bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr));
  if (rv) {
    std::cerr << "Failed to bind socket: " << errno << std::endl;
    return std::nullopt;
  }
  rv = listen(fd, SOMAXCONN);
  if (rv) {
    std::cerr << "Failed to listen on socket: " << errno << std::endl;
    return std::nullopt;
  }
  return fd;
}

int main(int, char **) {
  std::cout << "ReDiSxx" << std::endl;
  auto socket = create_socket();
  if (!socket) {
    std::cerr << "Failed to open socket." << std::endl;
    return -1;
  }
  std::cout << "Opened socket " << *socket << std::endl;
  const int kq_fd = kqueue();

  struct kevent evSet;
  EV_SET(&evSet, *socket, EVFILT_READ, EV_ADD, 0, 0, nullptr);
  assert(-1 != kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr));
  // while (1) {
  for (int i = 0; i < 10; ++i) {
    struct kevent events[32];
    const int num_events = kevent(kq_fd, nullptr, 0, events, 32, nullptr);
    std::cout << "Got " << num_events << " events." << std::endl;
    for (int i = 0; i < num_events; ++i) {
      const int in_fd = static_cast<int>(events[i].ident);
      std::cout << "Event " << i << " flags " << events[i].flags << " ident "
                << in_fd << std::endl;
      if (events[i].flags & EV_ADD && in_fd == *socket) {
        struct sockaddr_storage addr;
        socklen_t socklen = sizeof(addr);
        const int conn_fd =
            accept(in_fd, reinterpret_cast<struct sockaddr *>(&addr), &socklen);
        assert(conn_fd != -1);

        EV_SET(&evSet, conn_fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr);
        std::cout << "Connection established" << std::endl;

        assert(set_nonblocking(conn_fd));

        EV_SET(&evSet, conn_fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0,
               nullptr);
        kevent(kq_fd, &evSet, 1, nullptr, 0, nullptr);
      } else if (events[i].flags & EV_EOF) {
        std::cout << "Disconnected." << std::endl;
        close(in_fd);
      } else if (events[i].filter == EVFILT_READ) {
        char buffer[1024];
        const size_t bytes_read = recv(in_fd, buffer, sizeof(buffer), 0);
        std::cout << "Received " << bytes_read << " bytes:\n" << buffer << "\n";
      } else if(events[i].filter == EVFILT_WRITE) {
        std::cout << "Writing to socket.\n";
        const char *buffer = "Hello";
        const size_t bytes_written = send(in_fd, buffer, sizeof(buffer), 0);
        std::cout << "Wrote " << bytes_written << " bytes.\n";
      }
    }
  }
  return 0;
}