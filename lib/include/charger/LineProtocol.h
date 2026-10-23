#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace charger {

// The vehicle socket protocol: one command per line. This file only turns a
// line of text into a Command; it knows nothing about sockets or the state
// machine, so it is tested with plain strings. The table of commands and
// their effects is in docs/design.md, "Vehicle socket protocol".
struct Command {
    enum class Kind : std::uint8_t {
        Invalid,  // reason says why
        Plug,
        Unplug,
        Auth,
        Ready,
        Suspend,
        Fault,
        Clear,
        Current,  // amperes carries the value
        State,
    };

    Kind kind{Kind::Invalid};
    double amperes{0.0};  // Current only
    std::string reason;   // Invalid only; the text after "ERR " in the reply

    bool valid() const { return kind != Kind::Invalid; }
};

// Parses one line without its terminating '\n'. A trailing '\r' and
// surrounding spaces are ignored; everything else must match exactly.
Command parseCommand(std::string_view line);

// Largest current a command may request, in amperes. The cable rating of the
// bench charger; anything above it is refused at the protocol boundary.
constexpr double kMaxRequestedCurrentA = 80.0;

}  // namespace charger