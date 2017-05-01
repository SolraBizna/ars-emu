#ifndef UTFIT_HH
#define UTFIT_HH

#include "ars-emu.hh"

/* A way to iterate through the codepoints in a UTF-8 string, one by one. */

uint32_t getNextCodePoint(std::string::const_iterator& it,
                          std::string::const_iterator end);
#endif

