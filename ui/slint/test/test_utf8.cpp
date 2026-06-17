#include "utf8.h"
#include <string>
#include <cstdio>

static int check(const char *name, const std::string &got, const std::string &want) {
    if (got != want) {
        std::printf("FAIL %s: got %zu bytes, expected %zu\n", name, got.size(), want.size());
        return 1;
    }
    return 0;
}

int main() {
    int failures = 0;
    std::string s;

    s.clear(); zucs_to_utf8('A', s);        failures += check("ascii",   s, "A");
    s.clear(); zucs_to_utf8(0x00E9u, s);    failures += check("2-byte",  s, "\xC3\xA9");
    s.clear(); zucs_to_utf8(0x2022u, s);    failures += check("3-byte",  s, "\xE2\x80\xA2");
    s.clear(); zucs_to_utf8(0x1F600u, s);   failures += check("4-byte",  s, "\xF0\x9F\x98\x80");
    s.clear(); zucs_to_utf8(0x110000u, s);  failures += check("oob",     s, "");
    s.clear(); zucs_to_utf8(0xD800u, s);    failures += check("surrogate", s, "");

    if (failures == 0) {
        std::printf("test_utf8: all assertions passed\n");
        return 0;
    }
    std::printf("test_utf8: %d failure(s)\n", failures);
    return 1;
}
