#include <iostream>

#include "charger/StateMachine.h"

namespace {

// Stubs: stand-ins with canned answers, enough to run the state machine on a
// desk. The real implementations arrive with the protocols in weeks 3 and 4.
class StubVehicle final : public charger::VehicleInterface {
public:
    bool isPluggedIn() const override { return true; }
    double requestedCurrentA() const override { return 16.0; }
    bool isReady() const override { return true; }
};

class LoggingContactor final : public charger::ContactorInterface {
public:
    void close() override {
        closed_ = true;
        std::cout << " contactor: CLOSED\n";
    }

    void open() override {
        closed_ = false;
        std::cout << " contactor: OPEN\n";
    }

    bool isClosed() const override { return closed_; }

private:
    bool closed_{false};
};

}  // namespace

int main() {
    using charger::Event;

    const StubVehicle vehicle;
    LoggingContactor contactor;
    const charger::SystemClock clock;
    charger::StateMachine machine(vehicle, contactor, clock);

    // One complete session, including a pause requested by the vehicle.
    const Event script[] = {Event::PlugIn,  Event::Authorize, Event::EvReady, Event::EvSuspend,
                            Event::EvReady, Event::Authorize, Event::Unplug};

    std::cout << "charger-daemon: start in " << toString(machine.state()) << '\n';
    for (const Event event : script) {
        const charger::State before = machine.state();
        machine.handle(event);
        std::cout << toString(event) << ": " << toString(before) << " -> "
                  << toString(machine.state()) << '\n';
    }
    return 0;
}