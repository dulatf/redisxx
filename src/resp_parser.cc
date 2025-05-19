#include "resp_parser.h"

#include <iostream>
#include <variant>

#include "parsers.h"

// Language primitives
auto _sep_parser = parse_terminal("\r\n");
auto _str_len_parser =
    first(second(parse_char('$'), parse_uint()), _sep_parser);
auto _array_size_parser =
    first(second(parse_char('*'), parse_uint()), _sep_parser);

auto resp_bulk_string_parser =
    fmap(RespValue::make_string,
         first(fmap(
                   [](std::vector<char> input) {
                     return std::string(input.begin(), input.end());
                   },
                   repeat_n(_str_len_parser, parse_any_char())),
               _sep_parser));
auto resp_delimited_string_parser = fmap(
    RespValue::make_string,
    fmap(
        [](std::pair<std::vector<char>, std::string> input) {
          auto& [char_vec, term] = input;
          return std::string(char_vec.begin(), char_vec.end());
        },
        second(parse_char('+'),
               repeat_terminated(parse_any_char(), parse_terminal("\r\n")))));
auto resp_integer_parser =
    fmap(RespValue::make_integer,
         second(parse_char(':'), first(parse_int(), _sep_parser)));

std::optional<RespValue> parse_resp_code(std::string input) {
  std::function<ParseResult<RespValue>(std::string_view)>
      expr_parser_runner_impl;
  std::function<ParseResult<RespValue>(std::string_view)>
      array_parser_runner_impl;
  Parser<RespValue> expr_parser_recursive_ref(
      [&](std::string_view input) { return expr_parser_runner_impl(input); });
  Parser<RespValue> array_parser_recursive_ref(
      [&](std::string_view input) { return array_parser_runner_impl(input); });

  auto array_content_parser =
      repeat_n(_array_size_parser, expr_parser_recursive_ref);
  array_parser_runner_impl =
      fmap(RespValue::make_array, array_content_parser).run;

  expr_parser_runner_impl =
      or_else(resp_delimited_string_parser,
              or_else(resp_bulk_string_parser,
                      or_else(resp_integer_parser,
                              array_parser_recursive_ref  // The recursive part
                                                          // for nested arrays
                              )))
          .run;

  Parser<RespValue> resp_expr_parser(expr_parser_runner_impl);
  Parser<RespValue> resp_array_parser(array_parser_runner_impl);

  auto parse_result = resp_expr_parser.run(input);
  if (!parse_result) {
    return std::nullopt;
  } else {
    return std::get<0>(*parse_result);
  }
}