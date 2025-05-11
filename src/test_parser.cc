#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "parsers.h"

// Default template
template <typename T>
struct is_pair : std::false_type {};
// Specialization for std::pair
template <typename A, typename B>
struct is_pair<std::pair<A, B>> : std::true_type {};
// Helper variable template
template <typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

template <typename T>
struct is_vector : std::false_type {};
template <typename T, typename Alloc>  // Handle allocator template parameter
struct is_vector<std::vector<T, Alloc>> : std::true_type {};
template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

template <typename T>
void print_result(std::string_view test_name, std::string_view input_str,
                  const ParseResult<T>& result) {
  std::cout << "  " << test_name << "(\"" << input_str << "\") -> ";
  if (result) {
    std::cout << "Parsed: ";

    // Use the is_vector type trait to handle any std::vector<U>
    if constexpr (is_vector_v<T>) {
      std::cout << "[";
      bool first_elem = true;
      // Iterate over elements of any type U in std::vector<U>
      for (const auto& elem : result->first) {
        if (!first_elem) {
          std::cout << ", ";
        }
        // Print the element - assumes operator<< is defined for the element
        // type
        std::cout << elem;
        first_elem = false;
      }
      std::cout << "]";
    }
    // Check for pairs next
    else if constexpr (is_pair_v<T>) {
      std::cout << "(" << result->first.first << ", " << result->first.second
                << ")";
    }
    // Default output for other non-vector, non-pair types
    else {
      std::cout << result->first;
    }
    std::cout << ", Remaining: \"" << result->second << "\"" << std::endl;
  } else {
    std::cout << "Failed" << std::endl;
  }
}

int main(int argc, char** argv) {
  std::cout << "Testing parsers\n";
  auto star_parser = parse_char('*');
  auto digit_parser = parse_digit();

  print_result("star", "", star_parser.run(""));
  print_result("star", "*", star_parser.run("*"));
  print_result("digit", "", digit_parser.run(""));
  print_result("digit", "*", digit_parser.run("9"));

  auto uint_parser = parse_uint();
  print_result("number", "", uint_parser.run(""));
  print_result("number", "3", uint_parser.run("3"));
  print_result("number", "24", uint_parser.run("24"));
  print_result("number", "5381", uint_parser.run("5381"));

  auto eol_parser = and_then(parse_char('\r'), parse_char('\n'));
  auto array_size_parser =
      first(second(parse_char('*'), uint_parser), parse_terminal("\r\n"));

  print_result("array_def", "", array_size_parser.run(""));
  print_result("array_def", "*3", array_size_parser.run("*3"));
  print_result("array_def", "*10\r\n", array_size_parser.run("*10\r\n"));

  auto three_repeat_parser = repeat_n(pure<u_long>(3), parse_char('a'));
  print_result("three_repeat", "aa", three_repeat_parser.run("aa"));
  print_result("three_repeat", "aaa", three_repeat_parser.run("aaa"));
  auto dyn_repeat_parser = repeat_n(parse_uint(), parse_char('+'));
  print_result("dyn_repeat", "0", dyn_repeat_parser.run("0"));
  print_result("dyn_repeat", "4++", dyn_repeat_parser.run("4++"));
  print_result("dyn_repeat", "3+++", dyn_repeat_parser.run("3+++"));
  print_result("dyn_repeat", "2+++", dyn_repeat_parser.run("2+++"));

  auto sep_parser = parse_terminal("\r\n");
  auto str_len_parser =
      first(second(parse_char('$'), parse_uint()), sep_parser);
  auto bulk_string_parser =
      first(fmap(
                [](std::vector<char> input) {
                  return std::string(input.begin(), input.end());
                },
                repeat_n(str_len_parser, parse_any_char())),
            sep_parser);
  print_result("bulk_string_parser", "$3\r\nfoo\r\n",
               bulk_string_parser.run("$3\r\nfoo\r\n"));
  print_result("bulk_string_parser", "$3\r\nfo\r\n",
               bulk_string_parser.run("$3\r\nfo\r\n"));
  auto array_len_parser =
      first(second(parse_char('*'), parse_uint()), sep_parser);
  auto str_array_parser = repeat_n(array_len_parser, bulk_string_parser);
  print_result("array_parser", "*0\r\n", str_array_parser.run("*0\r\n"));
  print_result("array_parser", "*2\r\n$3\r\nfoo\r\n$6\r\nfoobar\r\n",
               str_array_parser.run("*2\r\n$3\r\nfoo\r\n$6\r\nfoobar\r\n"));

  auto delimited_string_parser = fmap(
      [](std::pair<std::vector<char>, std::string> input) {
        auto& [char_vec, term] = input;
        return std::string(char_vec.begin(), char_vec.end());
      },
      second(parse_char('+'),
             repeat_terminated(parse_any_char(), parse_terminal("\r\n"))));
  print_result("delim_str_parser", "+foobarn",
               delimited_string_parser.run("+foobarn"));
  print_result("delim_str_parser", "+foobar\r\n",
               delimited_string_parser.run("+foobar\r\n"));
  return 0;
}