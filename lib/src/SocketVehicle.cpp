#include "charger/SocketVehicle.h"

namespace charger {

std::optional<Event> SocketVehicle::apply(const Command& command) {
    using Kind = Command::Kind;
    switch (command.kind) {
        case Kind::Plug:
            pluggedIn_ = true;
            return Event::PlugIn;
        case Kind::Unplug:
            // A cable that is gone takes the ready signal with it.
            pluggedIn_ = false;
            ready_ = false;
            return Event::Unplug;
        case Kind::Auth:
            return Event::Authorize;
        case Kind::Ready:
            ready_ = true;
            return Event::EvReady;
        case Kind::Suspend:
            ready_ = false;
            return Event::EvSuspend;
        case Kind::Fault:
            return Event::Fault;
        case Kind::Clear:
            return Event::Clear;
        case Kind::Current:
            requestedCurrentA_ = command.amperes;
            return std::nullopt;
        case Kind::State:
        case Kind::Invalid:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace charger