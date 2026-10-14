#pragma once

namespace charger {

// What the charger can sense about the vehicle on the other end of the cable.
// The state machine depends on this abstraction, never on a concrete device, so
// the same logic runs against a mock (unit test), a simulated vehicle (SIL) or
// the real pilot-signal hardware (HIL and product).

class VehicleInterface {
public:
    virtual ~VehicleInterface() = default;

    // True while a connector is mated with the vehicle inlet.
    virtual bool isPluggedIn() const = 0;

    // Current the vehicle is asking for, in amperes.
    virtual double requestedCurrentA() const = 0;

    // True when the vehicle has signalled that it is ready to accept energy.
    virtual bool isReady() const = 0;
};

}  // namespace charger