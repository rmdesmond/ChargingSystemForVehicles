#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace charger {

// Tells the service manager (systemd) what state the daemon is in, using the
// sd_notify protocol: a short text datagram sent to the unix socket whose path
// systemd passes in the NOTIFY_SOCKET environment variable.
//
// Implemented directly on the socket API instead of linking libsystemd; see
// docs/adr/0002-sd-notify-without-libsystemd.md. When the daemon is not started
// by systemd there is no socket, and every call is a harmless no-op.
class ServiceNotifier {
public:
    // Reads NOTIFY_SOCKET and WATCHDOG_USEC from the environment.
    static ServiceNotifier fromEnvironment();

    // socketPath: empty means "not running under a service manager".
    // watchdogTimeout: zero means "no watchdog requested".
    ServiceNotifier(std::string socketPath, std::chrono::microseconds watchdogTimeout);

    bool enabled() const { return !socketPath_.empty(); }

    // The time systemd allows between two watchdog() calls before it declares
    // the service hung (WatchdogSec= in the unit file). Zero if not requested.
    std::chrono::microseconds watchdogTimeout() const { return watchdogTimeout_; }

    // Each returns true if the message was delivered.
    bool ready(); // start-up is complete (READY=1)
    bool watchdog(); // the main loop is alive (WATCHDOG=1)
    bool stopping(); // shutdown has begun (STOPPING=1)
private:
    bool send(std::string_view message) const;

    std::string socketPath_;
    std::chrono::microseconds watchdogTimeout_;
};

}  // namespace charger