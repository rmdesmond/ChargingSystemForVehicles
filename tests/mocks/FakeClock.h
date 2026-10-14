#pragma once

#include <chrono>

#include "charger/Clock.h"

// A fake, not a mock: a tiny working implementation whose time only moves when
// the test says so. Tests of timeouts become instant and exactly repeatable.
class FakeClock final : public charger::Clock {
public:
    TimePoint now() const override { return now_; }
    void advance(std::chrono::milliseconds duration) { now_ += duration; }

private:
    TimePoint now_{};
};