#include "charger/ServiceNotifier.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <utility>

namespace charger {

ServiceNotifier ServiceNotifier::fromEnvironment() {
    // getenv() is not thread-safe. This runs once, from main(), before any
    // other thread could exist.
    const char* socketPath = std::getenv("NOTIFY_SOCKET");
    const char* watchdogUsec = std::getenv("WATCHDOG_USEC");

    std::chrono::microseconds timeout{0};
    if (watchdogUsec != nullptr) {
        // Not a number, or zero: treated as "no watchdog".
        timeout = std::chrono::microseconds{std::strtoll(watchdogUsec, nullptr, 10)};
    }
    return {socketPath != nullptr ? socketPath : "", timeout};
}

ServiceNotifier::ServiceNotifier(std::string socketPath, std::chrono::microseconds watchdogTimeout)
    : socketPath_(std::move(socketPath)), watchdogTimeout_(watchdogTimeout) {}

bool ServiceNotifier::ready() { return send("READY=1"); }
bool ServiceNotifier::watchdog() { return send("WATCHDOG=1"); }
bool ServiceNotifier::stopping() { return send("STOPPING=1"); }

bool ServiceNotifier::send(std::string_view message) const {
    if (!enabled()) {
        return false;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socketPath_.size() >= sizeof(address.sun_path)) {
        return false;
    }
    std::memcpy(address.sun_path, socketPath_.data(), socketPath_.size());
    // A leading '@' names a socket in Linux's abstract namespace, which is
    // written on the wire as a leading zero byte.
    if (address.sun_path[0] == '@') {
        address.sun_path[0] = '\0';
    }
    const auto addressLength =
        static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + socketPath_.size());

    // A datagram socket: one send() is one complete message, no connection.
    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return false;
    }
    // sendto() takes the generic sockaddr type; this cast is how the API is used.
    const auto* genericAddress = reinterpret_cast<const sockaddr*>(&address);
    const ssize_t sent =
        ::sendto(fd, message.data(), message.size(), MSG_NOSIGNAL, genericAddress, addressLength);
    ::close(fd);
    return sent == static_cast<ssize_t>(message.size());
}

}  // namespace charger