#include "eval.hh"

#include <regex>
#include <iostream>
#include <assert.h>

namespace {
  enum class TokenType {
    OPERATOR, GROUP_BEGIN, GROUP_END, BANKED_ADDR, HEX_LITERAL, DEC_LITERAL,
      SYMBOL
  };
  const std::pair<std::regex, TokenType> lex[] = {
    {std::regex("8@|16@|24@|[-+/*%&|~^]|<<|>>"), TokenType::OPERATOR},
    {std::regex("\\("), TokenType::GROUP_BEGIN},
    {std::regex("\\)"), TokenType::GROUP_END},
    {std::regex("\\$([0-9A-Fa-f]{1,4})(?::([0-9A-Fa-f]{1,4})?)"),
     TokenType::BANKED_ADDR},
    {std::regex("\\$([0-9A-Fa-f]{1,8})"), TokenType::HEX_LITERAL},
    {std::regex("[0-9]{1,10}"), TokenType::DEC_LITERAL},
    {std::regex("[_a-zA-Z][_a-zA-Z0-9:]*"), TokenType::SYMBOL},
  };
  const struct binop {
    std::string token;
    int precedence;
    std::function<uint32_t(uint32_t,uint32_t)> func;
  } binary_operators[] = {
    {"-", 3, [](uint32_t a, uint32_t b) { return a-b; }},
    {"+", 3, [](uint32_t a, uint32_t b) { return a+b; }},
    {"*", 4, [](uint32_t a, uint32_t b) { return a*b; }},
    {"/", 4, [](uint32_t a, uint32_t b) { return a/b; }},
    {"%", 4, [](uint32_t a, uint32_t b) { return a%b; }},
    {"&", 2, [](uint32_t a, uint32_t b) { return a&b; }},
    {"|", 2, [](uint32_t a, uint32_t b) { return a|b; }},
    {"^", 2, [](uint32_t a, uint32_t b) { return a^b; }},
    {"<<", 1, [](uint32_t a, uint32_t b) { return a<<b; }},
    {">>", 1, [](uint32_t a, uint32_t b) { return a>>b; }},
  };
  const struct unop {
    std::string token;
    int precedence;
    std::function<uint32_t(uint32_t,
                           std::function<uint8_t(uint32_t)>)> func;
  } unary_operators[] = {
    {"-", 5, [](uint32_t x, std::function<uint8_t(uint32_t)>) { return -x; }},
    {"~", 5, [](uint32_t x, std::function<uint8_t(uint32_t)>) { return ~x; }},
    {"8@", 5, [](uint32_t x, std::function<uint8_t(uint32_t)> deref) {
        return deref(x);
      }},
    {"16@", 5, [](uint32_t x, std::function<uint8_t(uint32_t)> deref) {
        uint32_t ret = deref(x);
        ret |= static_cast<uint32_t>(deref(x+1))<<8;
        return ret;
      }},
    {"24@", 5, [](uint32_t x, std::function<uint8_t(uint32_t)> deref) {
        uint32_t ret = deref(x);
        ret |= static_cast<uint32_t>(deref(x+1))<<8;
        ret |= static_cast<uint32_t>(deref(x+2))<<16;
        return ret;
      }},
  };
  typedef std::vector<std::pair<TokenType, std::smatch> > lexvec;
  void print_error_position(const std::string& expression,
                            std::string::const_iterator pos) {
    std::cout << expression << "\n";
    while(pos != expression.cbegin()) {
      --pos;
      std::cout << "-";
    }
    std::cout << "^\n";
  }
  bool sub_eval(const std::string& expression,
                lexvec::const_iterator& it, lexvec::const_iterator end,
                std::function<bool(const std::string&, uint32_t&)> get_symbol,
                std::function<uint8_t(uint32_t)> read_address,
                uint32_t& out, int precedence = 0);
  bool eval_group(const std::string& expression,
                  lexvec::const_iterator& begin, lexvec::const_iterator end,
                  std::function<bool(const std::string&,uint32_t&)> get_symbol,
                  std::function<uint8_t(uint32_t)> read_address,
                  uint32_t& out) {
    auto it = ++begin;
    auto start = it;
    int depth = 1;
    while(it != end && depth > 0) {
      switch(it->first) {
      case TokenType::GROUP_BEGIN: ++depth; break;
      case TokenType::GROUP_END: --depth; break;
      default: break;
      }
      if(depth == 0) break;
      ++it;
    }
    if(it == end) {
      print_error_position(expression, expression.cend());
      std::cout << "Unbalanced parentheses\n";
      return false;
    }
    end = it;
    begin = ++it;
    return sub_eval(expression, start, end, get_symbol, read_address, out);
  }
  bool sub_eval(const std::string& expression,
                lexvec::const_iterator& it, lexvec::const_iterator end,
                std::function<bool(const std::string&, uint32_t&)> get_symbol,
                std::function<uint8_t(uint32_t)> read_address,
                uint32_t& out, int precedence) {
    assert(it != end);
    uint32_t val;
    /* parsing a left-hand value */
    switch(it->first) {
    case TokenType::OPERATOR: {
      std::remove_reference<decltype(*unary_operators)>::type* op = nullptr;
      for(auto& candidate : unary_operators) {
        if(it->second[0] == candidate.token) {
          op = &candidate;
          break;
        }
      }
      if(op == nullptr) {
        print_error_position(expression, it->second[0].first);
        std::cout << "Not a unary operator\n";
        return false;
      }
      ++it;
      if(it == end) {
        print_error_position(expression, expression.cend());
        std::cout << "Dangling unary operator\n";
        return false;
      }
      else if(!sub_eval(expression, it, end, get_symbol, read_address, val,
                        op->precedence))
        return false;
      val = op->func(val, read_address);
    } break;
    case TokenType::GROUP_BEGIN:
      if(!eval_group(expression, it, end, get_symbol, read_address, val))
        return false;
      break;
    case TokenType::BANKED_ADDR:
      /* unparseable hex numbers won't make it through the lexing process */
      val = (std::stoul(it->second[1], nullptr, 16) << 16)
        | std::stoul(it->second[2], nullptr, 16);
      ++it;
      break;
    case TokenType::HEX_LITERAL:
      val = std::stoul(it->second[1], nullptr, 16);
      ++it;
      break;
    case TokenType::DEC_LITERAL:
      /* unparseable decimal numbers, on the other hand... */
      try {
        val = std::stoul(it->second[0], nullptr, 10);
        ++it;
      }
      catch(std::out_of_range& e) {
        print_error_position(expression, it->second[0].first);
        std::cout << "Number out of range\n";
        return false;
      }
      break;
    case TokenType::SYMBOL:
      if(!get_symbol(it->second[0].str(), val)) {
        print_error_position(expression, it->second[0].first);
        std::cout << "Unknown symbol\n";
        return false;
      }
      ++it;
      break;
    default:
      print_error_position(expression, it->second[0].first);
      std::cout << "Unexpected token\n";
      return false;
    }
    while(it != end) {
      /* parsing a right-hand value (which must involve an intervening binary
         operator */
      switch(it->first) {
      case TokenType::OPERATOR: {
        uint32_t other_val;
        std::remove_reference<decltype(*binary_operators)>::type* op = nullptr;
        for(auto& candidate : binary_operators) {
          if(it->second[0] == candidate.token) {
            op = &candidate;
            break;
          }
        }
        if(op == nullptr) {
          print_error_position(expression, it->second[0].first);
          std::cout << "Not a binary operator\n";
          return false;
        }
        if(op->precedence < precedence) {
          out = val;
          return true;
        }
        ++it;
        if(it == end) {
          print_error_position(expression, expression.cend());
          std::cout << "Dangling binary operator\n";
          return false;
        }
        else if(!sub_eval(expression, it, end, get_symbol, read_address,
                          other_val, op->precedence))
          return false;
        val = op->func(val, other_val);
        break;
      }
      default:
        print_error_position(expression, it->second[0].first);
        std::cout << "Unexpected token\n";
        return false;
      }
    }
    assert(it == end);
    out = val;
    return true;
  }
}

bool eval(const std::string& expression,
          std::function<bool(const std::string&, uint32_t&)> get_symbol,
          std::function<uint8_t(uint32_t)> read_address,
          uint32_t& out) {
  lexvec lexed;
  auto lex_it = expression.cbegin();
  while(lex_it != expression.cend()) {
    while(lex_it != expression.cend() && *lex_it == ' ') ++lex_it;
    std::smatch match;
    bool matched = false;
    for(auto& lel : lex) {
      if(std::regex_search(lex_it, expression.cend(), match, lel.first,
                          std::regex_constants::match_continuous)) {
        matched = true;
        lex_it += match.length();
        lexed.emplace_back(lel.second, std::move(match));
        break;
      }
    }
    if(!matched) {
      print_error_position(expression, lex_it);
      std::cout << "Parse error\n";
      return false;
    }
  }
  if(lexed.size() == 0) {
    std::cout << "Empty expression\n";
    return false;
  }
  auto it = lexed.cbegin();
  return sub_eval(expression, it, lexed.cend(),
                  get_symbol, read_address, out);
}
