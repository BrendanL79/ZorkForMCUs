#include "fizmo_slint_bridge.h"
#include "utf8.h"
#include "fizmo_bridge.h"

#include <cstdint>
#include <vector>

std::string zork_drain_output() {
    std::string out;
    size_t avail = fizmo_output_available();
    if (avail == 0) {
        return out;
    }
    std::vector<std::uint32_t> buf(avail);
    size_t n = fizmo_output_read(buf.data(), buf.size());
    for (size_t i = 0; i < n; i++) {
        zucs_to_utf8(buf[i], out);
    }
    return out;
}
