#include <gtest/gtest.h>

#include "charger/StateMachine.h"

namespace {

// Hand-written stand-ins, just enough to construct a StateMachine. Step 3
// replaces them with GoogleMock classes.
struct ReadyVehicle final : charger::VehicleInterface {
    bool isPluggedIn() const override { return true; }
    double requestedCurrentA() const override { return 16.0; }
    bool isReady() const override { return true; }
};

struct NullContactor final : charger::ContactorInterface {
    void close() override {}
    void open() override {}
    bool isClosed() const override { return false; }
};

} // namespace

TEST(StateMachineSmokeTest, StartsAvailableAndReachesCharging) {
    const ReadyVehicle vehicle;
    NullContactor contactor;
    const charger::SystemClock clock;
    charger::StateMachine machine(vehicle, contactor, clock);

    EXPECT_EQ(machine.state(), charger::State::Available);
    EXPECT_TRUE(machine.handle(charger::Event::PlugIn));
    EXPECT_TRUE(machine.handle(charger::Event::Authorize));
    EXPECT_TRUE(machine.handle(charger::Event::EvReady));
    EXPECT_EQ(machine.state(), charger::State::Charging);
}