#ifndef EVAL_HH
#define EVAL_HH

#include <inttypes.h>
#include <string>
#include <functional>

bool eval(const std::string& expression,
          std::function<bool(const std::string&, uint32_t&)> get_symbol,
          std::function<uint8_t(uint32_t)> read_address,
          uint32_t& out);

#endif
