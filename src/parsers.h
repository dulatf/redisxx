#pragma once

#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

template <typename T>
using ParseResult = std::optional<std::pair<T, std::string_view>>;

template <typename T>
struct Parser {
  using ResultType = T;
  std::function<ParseResult<T>(std::string_view)> run;
  explicit Parser(std::function<ParseResult<T>(std::string_view)> f)
      : run(std::move(f)) {}

  // Prevent copy construction/assignment, allow move
  Parser(const Parser&) = default;
  Parser& operator=(const Parser&) = default;
  Parser(Parser&&) = default;
  Parser& operator=(Parser&&) = default;
};

template <typename Func>
auto make_parser(Func f) {
  // Deduce the full ParseResult type returned by the function f
  using PResultOpt = std::invoke_result_t<Func, std::string_view>;

  // Ensure it's an optional
  static_assert(std::is_same_v<PResultOpt,
                               std::optional<typename PResultOpt::value_type>>,
                "Function must return an std::optional type");

  // Extract the pair from the optional's value_type
  using PairType = typename PResultOpt::value_type;
  static_assert(
      std::is_same_v<
          PairType, std::pair<typename PairType::first_type, std::string_view>>,
      "Function must return std::optional<std::pair<T, std::string_view>>");

  // Extract the actual result type T
  using T = typename PairType::first_type;

  // Return the correctly typed Parser
  return Parser<T>(std::move(f));
}

// Monad unit or return.
template <typename T>
Parser<T> pure(T value) {
  return Parser<T>([v = std::move(value)](std::string_view input) {
    return std::make_pair(std::move(v), input);
  });
}

// Monad bind (`>>=`)
// Func: A -> Parser<B>
template <typename A, typename Func>
auto bind(Parser<A> parser_a, Func func) {
  using ParserB = std::invoke_result_t<Func, A>;
  using B = typename ParserB::ResultType;

  return Parser<B>([p_a = parser_a, f = std::move(func)](
                       std::string_view input) mutable -> ParseResult<B> {
    auto result_a = p_a.run(input);
    if (!result_a) {
      return std::nullopt;
    }
    A value_a = std::move(result_a->first);
    std::string_view remaining_input = std::move(result_a->second);
    Parser<B> parser_b = f(std::move(value_a));
    return parser_b.run(remaining_input);
  });
}

// fmap: Parser<A> -> Parser<B>
// Func: A->B
template <typename Func, typename A>
auto fmap(Func func, Parser<A> parser_a) {
  using B = std::invoke_result_t<Func, A>;
  return bind(parser_a, [f = std::move(func)](A val) mutable {
    B transformed_value = f(std::move(val));
    return pure<B>(std::move(transformed_value));
  });
}

template <typename A, typename B>
Parser<std::pair<A, B>> and_then(Parser<A> p_a, Parser<B> p_b) {
  return bind(p_a, [p_b](A a) mutable {
    // std::cout << "Parsed " << a << std::endl;
    return bind(p_b, [val_a = std::move(a)](B b) mutable {
      // std::cout << "Parsed " << b << std::endl;
      return pure(std::make_pair(std::move(val_a), std::move(b)));
    });
  });
}

template <typename T>
Parser<T> or_else(Parser<T> p1, Parser<T> p2) {
  return Parser<T>([p1, p2](std::string_view input) {
    auto result1 = p1.run(input);
    if (result1) {
      return result1;
    } else {
      return p2.run(input);
    }
  });
}

template <typename T>
Parser<std::vector<T>> many(Parser<T> parser) {
  return Parser<std::vector<T>>([p = parser](std::string_view input) mutable
                                    -> ParseResult<std::vector<T>> {
    std::vector<T> results;
    std::string_view current_input = input;
    while (true) {
      auto result = p.run(current_input);
      if (!result) {
        break;
      }
      if (result->second.length() == current_input.length()) {
        throw std::runtime_error(
            "Parser succeeded in `many` without consuming input");
      }
      results.push_back(std::move(result->first));
      current_input = result->second;
    }
    return std::make_pair(std::move(results), current_input);
  });
}

template <typename T>
Parser<std::vector<T>> one_or_more(Parser<T> parser) {
  return bind(
      parser, [p = parser](T first_result) mutable -> Parser<std::vector<T>> {
        return bind(many(p),
                    [first = std::move(first_result)](
                        std::vector<T> rest) mutable -> Parser<std::vector<T>> {
                      std::vector<T> combined;
                      combined.push_back(first);
                      combined.insert(combined.end(),
                                      std::make_move_iterator(rest.begin()),
                                      std::make_move_iterator(rest.end()));
                      return pure(std::move(combined));
                    });
      });
}

template <typename T>
Parser<std::vector<T>> repeat_n(Parser<u_long> count_parser,
                                Parser<T> item_parser) {
  return make_parser(
      [count_parser,
       item_parser](std::string_view input) -> ParseResult<std::vector<T>> {
        auto count_result = count_parser.run(input);
        if (!count_result) {
          return std::nullopt;
        }
        auto& [n, rest] = *count_result;

        std::vector<T> results;
        if (n > 0) {
          results.reserve(n);
        }
        std::string_view& current_rest = rest;
        for (u_long i = 0; i < n; ++i) {
          ParseResult<T> item_result = item_parser.run(current_rest);
          if (!item_result) {
            return std::nullopt;
          }
          auto& [item, next_rest] = *item_result;
          results.push_back(std::move(item));
          current_rest = next_rest;
        }
        return std::make_pair(std::move(results), current_rest);
      });
}

template <typename T, typename P>
Parser<std::pair<std::vector<T>, P>> repeat_terminated(
    Parser<T> item_parser, Parser<P> termination_parser) {
  return make_parser([termination_parser, item_parser](std::string_view input)
                         -> ParseResult<std::pair<std::vector<T>, P>> {
    if (input.empty()) {
      return std::nullopt;
    }
    std::vector<T> results;
    std::string_view current = input;

    while (true) {
      if (current.empty()) {
        return std::nullopt;
      }
      auto term_result = termination_parser.run(current);
      if (!term_result) {
        auto item_result = item_parser.run(current);
        if (!item_result) {
          return std::nullopt;
        }
        auto& [item, next_rest] = *item_result;
        results.push_back(std::move(item));
        current = next_rest;
        // continue
      } else {
        auto& [termination_parse, termination_rest] = *term_result;
        // we're done
        return std::make_pair(
            std::make_pair(std::move(results), termination_parse),
            termination_rest);
      }
    }
  });
}

// Convenience Combinators

template <typename A, typename B>
Parser<A> first(Parser<A> p_a, Parser<B> p_b) {
  return bind(p_a, [p_b](A a) mutable {
    return bind(p_b, [val_a = std::move(a)](B b) mutable {
      return pure(std::move(val_a));
    });
  });
}

template <typename A, typename B>
Parser<B> second(Parser<A> p_a, Parser<B> p_b) {
  return bind(p_a, [p_b](A a) mutable {
    return bind(p_b, [](B b) mutable { return pure(std::move(b)); });
  });
}

template <typename T>
Parser<std::optional<T>> maybe(Parser<T> parser) {
  return make_parser(
      [parser](std::string_view input) -> ParseResult<std::optional<T>> {
        auto inner = parser.run(input);
        if (!inner) {
          return std::make_pair(std::nullopt, input);
        } else {
          return inner;
        }
      });
}

// Primitives
inline Parser<char> parse_char(char expected) {
  return make_parser([expected](std::string_view input) -> ParseResult<char> {
    if (!input.empty() && input.front() == expected) {
      return std::make_pair(expected, input.substr(1));
    }
    return std::nullopt;
  });
}

inline Parser<char> parse_any_char() {
  return make_parser([](std::string_view input) -> ParseResult<char> {
    if (!input.empty()) {
      return std::make_pair(input.front(), input.substr(1));
    }
    return std::nullopt;
  });
}

inline Parser<std::string> parse_terminal(const std::string& expected) {
  return make_parser(
      [expected](std::string_view input) -> ParseResult<std::string> {
        if (input.starts_with(expected)) {
          return std::make_pair(expected, input.substr(expected.length()));
        }
        return std::nullopt;
      });
}

inline Parser<int> parse_digit() {
  return make_parser([](std::string_view input) -> ParseResult<int> {
    if (!input.empty() && std::isdigit(input.front())) {
      return std::make_pair(input.front() - '0', input.substr(1));
    }
    return std::nullopt;
  });
}

inline Parser<u_long> parse_uint() {
  return fmap(
      [](const std::vector<int>& input) -> u_long {
        u_long acc = 0;
        for (int i = 0; i < input.size(); ++i) {
          int digit = input[i];
          int exponent = input.size() - i - 1;
          acc += digit * powl(10, exponent);
        }
        return acc;
      },
      one_or_more(parse_digit()));
}

inline Parser<long> parse_int() {
  return fmap(
      [](const std::pair<std::optional<char>, u_long>& input) {
        auto& [sign, number] = input;
        if (sign && *sign == '-') {
          return -static_cast<long>(number);
        } else {
          return static_cast<long>(number);
        }
      },
      and_then(maybe(or_else(parse_char('+'), parse_char('-'))), parse_uint()));
}
