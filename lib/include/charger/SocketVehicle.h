#pragma once

#include <optional>

#include "charger/LineProtocol.h"
#include "charger/StateMachine.h"
#include "charger/VehicleInterface.h"

namespace charger {

// A vehicle whose plug, ready and requested-current signals are set by
// commands from the harness instead of by pilot-signal hardware. It owns no
// socket: the daemon's main loop reads lines, parses them, and calls apply().
//
// apply() does two things in a fixed order: first it updates the signals the
// state machine will read, then it names the event the state machine should
// handle. That order matters: when the state machine acts on EvReady it checks
// isPluggedIn() and isReady() (REQ-007), and those must already be true.
class SocketVehicle final : public VehicleInterface {
public:
    bool isPluggedIn() const override { return pluggedIn_; }
    double requestedCurrentA() const override { return requestedCurrentA_; }
    bool isReady() const override { return ready_; }

    // Updates the signals and returns the event to raise, if the command has
    // one. Invalid commands and CURRENT/STATE raise nothing.
    std::optional<Event> apply(const Command& command);

private:
    bool pluggedIn_{false};
    bool ready_{false};
    double requestedCurrentA_{0.0};
};

}  // namespace charger