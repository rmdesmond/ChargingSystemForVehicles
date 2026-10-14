#pragma once

#include <chrono>

#include "charger/Clock.h"
#include "charger/ContactorInterface.h"
#include "charger/VehicleInterface.h"

namespace charger {

// Must match the tables in docs/design.md.
enum class State { Available, Preparing, Charging, SuspendedEV, Finishing, Faulted };
enum class Event { PlugIn, Authorize, EvReady, EvSuspend, Unplug, Fault, Clear };

const char* toString(State state);
const char* toString(Event event);

// The charging session state machine. It owns no hardware and no thread: every
// input arrives through handle() or tick(), and every output goes through the
// interfaces passed to the constructor.

class StateMachine {
public:
    // REQ-006: how long Preparing may last without authorization.
    static constexpr std::chrono::seconds kPreparingTimeout{60};

    // The referenced objects must outlive the state machine.
    StateMachine(const VehicleInterface& vehicle, ContactorInterface& contactor,
                 const Clock& clock);

    // Apply one event. Returns true if it caused a transition, false if the
    // event is not valid in the current state and was ignored.
    bool handle(Event event);

    // Advance time-based behaviour. Call periodically (the daemon will call it
    // about once a second).
    void tick();

    State state() const { return state_; }

private:
    enum class Action { None, CloseContactor, OpenContactor };
    struct Transition {
        State from;
        Event event;
        State to;
        Action action;
    };
    static const Transition kTransitions[];

    void enter(State next, Action action);

    const VehicleInterface& vehicle_;
    ContactorInterface& contactor_;
    const Clock& clock_;
    State state_{State::Available};
    Clock::TimePoint enteredAt_;
};

}  // namespace charger