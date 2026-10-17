#include <csignal>
#include <ctime>
#include <iostream>
#include <string_view>

#include "charger/ServiceNotifier.h"
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
        std::cerr << " contactor: CLOSED\n";
    }
    void open() override {
        closed_ = false;
        std::cerr << " contactor: OPEN\n";
    }
    bool isClosed() const override { return closed_; }

private:
    bool closed_{false};
};

// --demo: one scripted session, including a pause requested by the vehicle.
int runDemo(charger::StateMachine& machine) {
    using charger::Event;
    const Event script[] = {Event::PlugIn,  Event::Authorize, Event::EvReady, Event::EvSuspend,
                            Event::EvReady, Event::Authorize, Event::Unplug};

    for (const Event event : script) {
        const charger::State before = machine.state();
        machine.handle(event);
        std::cerr << toString(event) << ": " << toString(before) << " -> "
                  << toString(machine.state()) << '\n';
    }
    return 0;
}

// The service loop: once a second advance the state machine and, if a service
// manager asked for it, report that the loop is still alive.
int runService(charger::StateMachine& machine, charger::ServiceNotifier& notifier) {
    // Block the termination signals and collect them synchronously below. No
    // signal handler runs, so nothing here has to be async-signal-safe.
    sigset_t stopSignals;
    sigemptyset(&stopSignals);
    sigaddset(&stopSignals, SIGTERM);
    sigaddset(&stopSignals, SIGINT);
    sigprocmask(SIG_BLOCK, &stopSignals, nullptr);

    // Ping at half the timeout, as systemd recommends, counted in loop periods.
    using std::chrono::seconds;
    const auto watchdogTimeout = std::chrono::duration_cast<seconds>(notifier.watchdogTimeout());
    const auto pingEvery = watchdogTimeout.count() >= 2 ? watchdogTimeout.count() / 2 : 1;

    notifier.ready();
    std::cerr << "charger-daemon: running; service manager "
              << (notifier.enabled() ? "notified" : "not present") << ", watchdog "
              << watchdogTimeout.count() << " s\n";

    for (long long period = 1;; ++period) {
        const timespec onePeriod{1, 0};
        // Sleeps for one period, or returns at once with the signal number.
        const int signal = sigtimedwait(&stopSignals, nullptr, &onePeriod);
        if (signal > 0) {
            std::cerr << "charger-daemon: signal " << signal << ", stopping\n";
            break;
        }
        machine.tick();
        if (period % pingEvery == 0) {
            notifier.watchdog();
        }
    }

    notifier.stopping();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const StubVehicle vehicle;
    LoggingContactor contactor;
    const charger::SystemClock clock;
    charger::StateMachine machine(vehicle, contactor, clock);

    // std::cerr is unbuffered, so every line reaches the journal immediately.
    std::cerr << "charger-daemon: start in " << toString(machine.state()) << '\n';
    if (argc == 1) {
        charger::ServiceNotifier notifier = charger::ServiceNotifier::fromEnvironment();
        return runService(machine, notifier);
    }
    if (argc == 2 && std::string_view{argv[1]} == "--demo") {
        return runDemo(machine);
    }
    std::cerr << "usage: charger-daemon [--demo]\n";
    return 2;
}