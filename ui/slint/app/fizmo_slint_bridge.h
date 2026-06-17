#ifndef FIZMO_SLINT_BRIDGE_H
#define FIZMO_SLINT_BRIDGE_H

#include <string>

// Drain all currently-available interpreter output from the fizmo bridge,
// returning it as a UTF-8 string (empty if nothing is available).
std::string zork_drain_output();

#endif
