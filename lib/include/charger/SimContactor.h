#pragma once

#include "charger/ContactorInterface.h"

namespace charger {

// The contactor until the safety MCU owns it in week 4: a flag and a log
// line per actuation, so the journal and the SIL suite can see what the
// state machine commanded. With stuck=true, close() is commanded but the
// device never reports closed, which is the fault REQ-011 must catch.
class SimContactor final : public ContactorInterface {
public:
    explicit SimContactor(bool stuck = false) : stuck_(stuck) {}

    void close() override;
    void open() override;
    bool isClosed() const override { return closed_; }

private:
    bool stuck_;
    bool closed_{false};
};

}  // namespace charger