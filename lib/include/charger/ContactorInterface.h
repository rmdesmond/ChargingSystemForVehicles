#pragma once

namespace charger {

// The power relay between the grid and the cable. Closed means the cable is
// live. Opening it is the safe state for every fault in this system.

class ContactorInterface {
public:
    virtual ~ContactorInterface() = default;

    // Command the contactor closed (cable becomes live).
    virtual void close() = 0;

    // Command the contactor open (cable de-energised). Must be safe to call
    // repeatedly: the state machine calls it without checking the current state.
    virtual void open() = 0;

    // Feedback from the device: is it actually closed?
    virtual bool isClosed() const = 0;
};
}  // namespace charger