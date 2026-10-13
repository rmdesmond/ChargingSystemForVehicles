#pragma once

#include <string>

namespace charger {

// Placeholder for the charging state machine that arrives on Wednesday.
class Charger {
public:
    // Text the daemon logs at start-up.
    std::string greeting() const;
};

}  // namespace charger