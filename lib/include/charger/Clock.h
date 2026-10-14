#pragma once

#include <chrono>

namespace charger {

// Time is a dependency like any other. Code that asks this interface for the
// time, instead of calling the operating system directly, can be tested with a
// clock the test controls: a 60 second timeout is tested in microseconds.

class Clock {
public:
    // Monotonic time: never jumps when the wall clock is adjusted (NTP, DST).
    using TimePoint = std::chrono::steady_clock::time_point;

    virtual ~Clock() = default;
    virtual TimePoint now() const = 0;
};

// The production clock.
class SystemClock final : public Clock {
public:
    TimePoint now() const override { return std::chrono::steady_clock::now(); }
};

}  // namespace charger