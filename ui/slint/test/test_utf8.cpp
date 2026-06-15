#include "utf8.h"
#include <cassert>
#include <string>
#include <cstdio>

int main() {
    std::string s;

    s.clear(); zucs_to_utf8('A', s);            // 1-byte ASCII
    assert(s == "A");

    s.clear(); zucs_to_utf8(0x00E9u, s);        // é  -> C3 A9
    assert(s == "\xC3\xA9");

    s.clear(); zucs_to_utf8(0x2022u, s);        // •  -> E2 80 A2
    assert(s == "\xE2\x80\xA2");

    s.clear(); zucs_to_utf8(0x1F600u, s);       // 😀 -> F0 9F 98 80
    assert(s == "\xF0\x9F\x98\x80");

    s.clear(); zucs_to_utf8(0x110000u, s);      // out of range -> skipped
    assert(s.empty());

    std::printf("test_utf8: all assertions passed\n");
    return 0;
}
