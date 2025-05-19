#include "connection.h"

#include <span>

#include "commands.h"
#include "resp_parser.h"

EventState handle_read(Connection &con) {
  std::cout << "Handling `read` on socket " << con.fd << std::endl;
  uint8_t buffer[1024] = {0};
  const size_t bytes_read = recv(con.fd, buffer, sizeof(buffer), 0);
  std::cout << "Received " << bytes_read << " bytes:\n"
            << (const char *)buffer << "\n";
  con.incoming.insert(con.incoming.end(), &buffer[0], &buffer[0] + bytes_read);

  // Attempt to parse
  std::string input_str(con.incoming.begin(), con.incoming.end());
  auto parsed = parse_resp_code(input_str);
  if (!parsed) {
    return EventState::Read;
  }
  // Remove parsed bytes from input stream
  // TODO: Return unparsed bytes from parser and keep those in stream.
  con.incoming.erase(con.incoming.begin(),
                     con.incoming.begin() + input_str.size());
  con.outgoing.clear();

  std::cout << "Parsed: " << parsed->to_string() << std::endl;
  auto command_args = parsed->to_array_safe();
  if (command_args.empty()) {
    std::cout << "Empty command\n";
    return EventState::Idle;
  }
  if (!std::holds_alternative<RespString>(command_args.front().value)) {
    std::cerr << "Unexpected entry, command string expected: "
              << command_args.front().to_string() << std::endl;
    return EventState::Idle;
  }
  const std::string command_string = command_args.front().to_string();
  command_args.erase(command_args.begin());
  auto command_response = dispatch_commands(command_string, command_args);
  if (!command_response) {
    std::cerr << "Failed to handle command `" << command_string << "`\n";
    return EventState::Idle;
  }
  // Convert response into protocol representation and add to outgoing buffer.
  const auto command_response_encoded =
      command_response->to_protocol_representation();
  std::copy(command_response_encoded.begin(), command_response_encoded.end(),
            std::back_inserter(con.outgoing));
  if (con.outgoing.empty()) {
    return EventState::Idle;
  }
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