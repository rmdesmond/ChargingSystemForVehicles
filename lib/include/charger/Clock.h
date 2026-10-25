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

// A clock that runs faster than another one, for tests of time-based
// behaviour against the real binary: at scale 20, the 60 s Preparing timeout
// arrives after 3 s. Only the state machine ever sees this clock; anything
// with a real-time contract to another process (the service manager's
// watchdog, week 4's heartbeat) keeps real time. See ADR 0003.
class ScaledClock final : public Clock {
public:
    ScaledClock(const Clock& base, double scale)
        : base_(base), scale_(scale), origin_(base.now()) {}

    TimePoint now() const override {
        const auto real = base_.now() - origin_;
        return origin_ + std::chrono::duration_cast<TimePoint::duration>(real * scale_);
    }

private:
    const Clock& base_;
    double scale_;
    TimePoint origin_;
};

}  // namespace charger