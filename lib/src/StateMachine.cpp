#include "charger/StateMachine.h"

namespace charger {

// The whole behaviour of the charger, as data. One row per legal transition;
// any (state, event) pair that is not listed is ignored. Keep this identical to
// the table in docs/design.md. The columns are aligned by hand because people
// read this table, so the formatter is switched off around it.
// clang-format off
const StateMachine::Transition StateMachine::kTransitions[] = {
    // from event to action
    {State::Available, Event::PlugIn, State::Preparing, Action::None},
    {State::Preparing, Event::Authorize, State::SuspendedEV, Action::None},
    {State::Preparing, Event::Unplug, State::Available, Action::None},
    {State::SuspendedEV, Event::EvReady, State::Charging, Action::CloseContactor},
    {State::SuspendedEV, Event::Authorize, State::Finishing, Action::None},
    {State::SuspendedEV, Event::Unplug, State::Available, Action::None},
    {State::Charging, Event::EvSuspend, State::SuspendedEV, Action::OpenContactor},
    {State::Charging, Event::Authorize, State::Finishing, Action::OpenContactor},
    {State::Charging, Event::Unplug, State::Available, Action::OpenContactor},
    {State::Finishing, Event::Unplug, State::Available, Action::None},
    // REQ-004: a fault is accepted in every state and always opens the contactor.
    {State::Available, Event::Fault, State::Faulted, Action::OpenContactor},
    {State::Preparing, Event::Fault, State::Faulted, Action::OpenContactor},
    {State::Charging, Event::Fault, State::Faulted, Action::OpenContactor},
    {State::SuspendedEV, Event::Fault, State::Faulted, Action::OpenContactor},
    {State::Finishing, Event::Fault, State::Faulted, Action::OpenContactor},
    {State::Faulted, Event::Fault, State::Faulted, Action::OpenContactor},
    // REQ-005: Clear is the only way out of Faulted.
    {State::Faulted, Event::Clear, State::Available, Action::None},
};
// clang-format on

StateMachine::StateMachine(const VehicleInterface& vehicle, ContactorInterface& contactor, const Clock& clock)
    : vehicle_(vehicle), contactor_(contactor), clock_(clock), enteredAt_(clock.now()) {}

bool StateMachine::handle(Event event) {
    for (const Transition& t : kTransitions) {
        if (t.from == state_ && t.event == event) {
            enter(t.to, t.action);
            return true;
        }
    }
    return false;
}

void StateMachine::tick() {
    // REQ-006: nobody authorized within the timeout; give the connector back.
    if (state_ == State::Preparing && clock_.now() - enteredAt_ >= kPreparingTimeout) {
        enter(State::Available, Action::None);
    }
}

void StateMachine::enter(State next, Action action) {
    switch (action) {
        case Action::CloseContactor:
            // REQ-007: never energise the cable on the strength of an event
            // alone. If the vehicle interface disagrees, fail to the safe state.
            if (vehicle_.isPluggedIn() && vehicle_.isReady()) {
                contactor_.close();
            } else {
                contactor_.open();
                next = State::Faulted;
            }
            break;
        case Action::OpenContactor:
            contactor_.open();
            break;
        case Action::None:
            break;
    }
    state_ = next;
    enteredAt_ = clock_.now();
}

// No default label on purpose: with -Wall -Werror, adding an enumerator without
// handling it here is a compile error.
const char* toString(State state) {
    switch (state) {
        case State::Available:
           return "Available";
        case State::Preparing:
            return "Preparing";
        case State::Charging:
            return "Charging";
        case State::SuspendedEV:
            return "SuspendedEV";
        case State::Finishing:
            return "Finishing";
        case State::Faulted:
            return "Faulted";
    }
    return "?";
}

const char* toString(Event event) {
    switch (event) {
        case Event::PlugIn:
            return "PlugIn";
        case Event::Authorize:
            return "Authorize";
        case Event::EvReady:
            return "EvReady";
        case Event::EvSuspend:
            return "EvSuspend";
        case Event::Unplug:
            return "Unplug";
        case Event::Fault:
            return "Fault";
        case Event::Clear:
            return "Clear";
    }
    return "?";
}

} // namespace charger