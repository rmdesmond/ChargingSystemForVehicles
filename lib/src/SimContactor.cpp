#include "charger/SimContactor.h"

#include <iostream>

namespace charger {

void SimContactor::close() {
    if (stuck_) {
        // Commanded, but the device did not move. isClosed() stays false.
        std::cerr << " contactor: CLOSE FAILED (stuck)\n";
        return;
    }
    closed_ = true;
    std::cerr << " contactor: CLOSED\n";
}

void SimContactor::open() {
    closed_ = false;
    std::cerr << " contactor: OPEN\n";
}

}  // namespace charger