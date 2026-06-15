#ifndef ZORK_UTF8_H
#define ZORK_UTF8_H

#include <cstdint>
#include <string>

// Append the UTF-8 encoding of a single Unicode code point to `out`.
// Code points outside the valid range (> U+10FFFF) are skipped.
void zucs_to_utf8(std::uint32_t cp, std::string &out);

#endif
