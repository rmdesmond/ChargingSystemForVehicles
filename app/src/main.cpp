#include <poll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "charger/LineProtocol.h"
#include "charger/LineServer.h"
#include "charger/ServiceNotifier.h"
#include "charger/SimContactor.h"
#include "charger/SocketVehicle.h"
#include "charger/StateMachine.h"

namespace {

// Stub: a vehicle with canned answers, enough to run the state machine on a
// desk with no harness attached. Replaced by SocketVehicle under SIL and by
// the MCU link in week 4.
class StubVehicle final : public charger::VehicleInterface {
public:
    bool isPluggedIn() const override { return true; }
    double requestedCurrentA() const override { return 16.0; }
    bool isReady() const override { return true; }
};

// Everything the command line can set. Parsed once in main().
struct Options {
    bool demo{false};
    std::string vehicleSocket;  // empty: StubVehicle
    std::string statusSocket;   // empty: no status port
    bool sim{false};            // read simulation controls from the environment
    double timeScale{1.0};      // >1: the state machine's clock runs faster (test only)
};

constexpr std::string_view kUsage =
    "usage: charger-daemon [--demo] [--vehicle-socket PATH] [--status-socket PATH]\n"
    "                      [--sim] [--time-scale N]\n"
    "  --sim           simulation controls from the environment, e.g.\n"
    "                  CHARGER_SIM_CONTACTOR=stuck: close() never reports closed\n"
    "  --time-scale N  state machine timeouts run N times faster (1 to 1000); test only\n";
constexpr double kMaxTimeScale = 1000.0;

// Returns nullopt on a usage error; the usage line has been printed.
std::optional<Options> parseOptions(int argc, char** argv) {
    Options options;
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool hasValue = i + 1 < args.size();
        if (arg == "--demo") {
            options.demo = true;
        } else if (arg == "--vehicle-socket" && hasValue) {
            options.vehicleSocket = args[++i];
        } else if (arg == "--status-socket" && hasValue) {
            options.statusSocket = args[++i];
        } else if (arg == "--sim") {
            options.sim = true;
        } else if (arg == "--time-scale" && hasValue) {
            char* end = nullptr;
            options.timeScale = std::strtod(std::string(args[++i]).c_str(), &end);
            if (*end != '\0' || options.timeScale < 1.0 || options.timeScale > kMaxTimeScale) {
                std::cerr << kUsage;
                return std::nullopt;
            }
        } else {
            std::cerr << kUsage;
            return std::nullopt;
        }
    }
    return options;
}

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

// The two harness sockets and what each one says. Either may be absent.
class HarnessPorts {
public:
    HarnessPorts(charger::StateMachine& machine, charger::SocketVehicle& vehicle)
        : machine_(machine), vehicle_(vehicle) {}

    // Returns false if a requested socket could not be opened (already logged).
    bool open(const Options& options) {
        // 0600: the vehicle socket is a command channel for the harness only.
        // 0666: anyone on the device may read the state.
        return listen(vehicle_server_, options.vehicleSocket, 0600, false, "vehicle") &&
               listen(status_server_, options.statusSocket, 0666, true, "status");
    }

    void addTo(std::vector<pollfd>& fds) {
        for (const auto& server : {&vehicle_server_, &status_server_}) {
            if (*server) {
                (*server)->addTo(fds);
            }
        }
    }

    void service(const std::vector<pollfd>& fds) {
        const auto stateLine = [this] {
            return "STATE " + std::string(toString(machine_.state()));
        };
        if (vehicle_server_) {
            vehicle_server_->service(
                fds, [] { return std::string{}; },
                [this, &stateLine](std::string_view line) { return handle(line, stateLine); });
        }
        if (status_server_) {
            status_server_->service(fds, stateLine, [](std::string_view) { return std::string{}; });
        }
    }

private:
    static bool listen(std::unique_ptr<charger::LineServer>& server, const std::string& path,
                       mode_t mode, bool closeAfterGreeting, const char* name) {
        if (path.empty()) {
            return true;
        }
        server = std::make_unique<charger::LineServer>(path, mode, closeAfterGreeting);
        if (!server->listening()) {
            std::cerr << "charger-daemon: cannot listen on " << path << '\n';
            return false;
        }
        std::cerr << "charger-daemon: " << name << " socket " << path << '\n';
        return true;
    }

    // One command line in, one reply line out. The vehicle is updated before
    // the event is handled, so REQ-007's check sees the new plug/ready state.
    template <typename StateLine>
    std::string handle(std::string_view line, const StateLine& stateLine) {
        const charger::Command command = charger::parseCommand(line);
        if (!command.valid()) {
            return "ERR " + command.reason;
        }
        if (const auto event = vehicle_.apply(command)) {
            const charger::State before = machine_.state();
            machine_.handle(*event);
            std::cerr << toString(*event) << ": " << toString(before) << " -> "
                      << toString(machine_.state()) << '\n';
        }
        return stateLine();
    }

    charger::StateMachine& machine_;
    charger::SocketVehicle& vehicle_;
    std::unique_ptr<charger::LineServer> vehicle_server_;
    std::unique_ptr<charger::LineServer> status_server_;
};

// Opens a signalfd for SIGTERM and SIGINT. The signals are blocked, so they
// no longer interrupt anything; they are read from the descriptor like data.
int openStopSignals() {
    sigset_t stopSignals;
    sigemptyset(&stopSignals);
    sigaddset(&stopSignals, SIGTERM);
    sigaddset(&stopSignals, SIGINT);
    sigprocmask(SIG_BLOCK, &stopSignals, nullptr);
    return signalfd(-1, &stopSignals, SFD_NONBLOCK | SFD_CLOEXEC);
}

// The service loop. One thread, one poll(): the stop signals, the harness
// sockets and the state machine's tick timer all arrive here.
int runService(charger::StateMachine& machine, charger::ServiceNotifier& notifier,
               HarnessPorts& ports, double timeScale) {
    using Clock = std::chrono::steady_clock;
    using std::chrono::milliseconds;

    const int signalFd = openStopSignals();

    // Ping at half the timeout, as systemd recommends. Zero means no watchdog.
    // Real time, whatever the time scale: this is a contract with systemd.
    const auto pingEvery = std::chrono::duration_cast<milliseconds>(notifier.watchdogTimeout()) / 2;
    // The state machine is ticked once per second of ITS time, so a scaled
    // clock also scales how often timeouts are checked.
    const milliseconds kTickPeriod{static_cast<long>(1000.0 / timeScale)};

    notifier.ready();
    std::cerr
        << "charger-daemon: running; service manager "
        << (notifier.enabled() ? "notified" : "not present") << ", watchdog "
        << std::chrono::duration_cast<std::chrono::seconds>(notifier.watchdogTimeout()).count()
        << " s\n";

    auto nextTick = Clock::now() + kTickPeriod;
    auto lastPing = Clock::now();
    std::vector<pollfd> fds;
    for (;;) {
        fds.clear();
        fds.push_back(pollfd{signalFd, POLLIN, 0});
        ports.addTo(fds);

        // Sleep until something arrives, or until the next tick is due.
        const auto untilTick = std::chrono::duration_cast<milliseconds>(nextTick - Clock::now());
        poll(fds.data(), fds.size(), static_cast<int>(std::max<long>(untilTick.count(), 0)));

        if ((fds[0].revents & POLLIN) != 0) {
            signalfd_siginfo info{};
            const ssize_t got = read(signalFd, &info, sizeof(info));
            std::cerr << "charger-daemon: signal " << (got > 0 ? info.ssi_signo : 0U)
                      << ", stopping\n";
            break;
        }
        ports.service(fds);

        const auto now = Clock::now();
        if (now >= nextTick) {
            machine.tick();
            nextTick += kTickPeriod;
        }
        if (pingEvery.count() > 0 && now - lastPing >= pingEvery) {
            notifier.watchdog();
            lastPing = now;
        }
    }

    notifier.stopping();
    close(signalFd);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::optional<Options> options = parseOptions(argc, argv);
    if (!options) {
        return 2;
    }

    // The vehicle: the stub, or the one the harness drives over a socket.
    const StubVehicle stubVehicle;
    charger::SocketVehicle socketVehicle;
    const charger::VehicleInterface& vehicle =
        options->vehicleSocket.empty() ? static_cast<const charger::VehicleInterface&>(stubVehicle)
                                       : socketVehicle;

    //  The contactor: simulated until week 4. Under --sim it can be made to
    //  fail from the environment, which is how the SIL suite injects faults.
    const char* simContactor = options->sim ? std::getenv("CHARGER_SIM_CONTACTOR") : nullptr;
    charger::SimContactor contactor(simContactor != nullptr &&
                                    std::string_view{simContactor} == "stuck");

    const charger::SystemClock systemClock;
    const charger::ScaledClock scaledClock(systemClock, options->timeScale);
    const charger::Clock& clock =
        options->timeScale == 1.0 ? static_cast<const charger::Clock&>(systemClock) : scaledClock;
    charger::StateMachine machine(vehicle, contactor, clock);
    if (options->timeScale != 1.0) {
        std::cerr << "charger-daemon: time scale " << options->timeScale << " (test only)\n";
    }

    // std::cerr is unbuffered, so every line reaches the journal immediately.
    std::cerr << "charger-daemon: start in " << toString(machine.state()) << '\n';

    if (options->demo) {
        return runDemo(machine);
    }

    // Sockets are opened before READY=1, so a client that waits for the
    // service to become active finds them in place.
    HarnessPorts ports(machine, socketVehicle);
    if (!ports.open(*options)) {
        return 1;
    }
    charger::ServiceNotifier notifier = charger::ServiceNotifier::fromEnvironment();
    return runService(machine, notifier, ports, options->timeScale);
}
