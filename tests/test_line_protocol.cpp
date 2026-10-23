#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "charger/LineProtocol.h"
#include "charger/SocketVehicle.h"

using charger::Command;
using charger::Event;
using charger::parseCommand;
using charger::SocketVehicle;
using Kind = Command::Kind;

namespace {

// ---------------------------------------------------------------------------
// REQ-012: the parser. Every accepted spelling, then everything refused.
// ---------------------------------------------------------------------------

struct Spelling {
    const char* line;
    Kind kind;
};

class AcceptedLineTest : public ::testing::TestWithParam<Spelling> {};

TEST_P(AcceptedLineTest, ParsesToItsCommand) {
    const Command command = parseCommand(GetParam().line);
    EXPECT_TRUE(command.valid()) << command.reason;
    EXPECT_EQ(command.kind, GetParam().kind);
}

INSTANTIATE_TEST_SUITE_P(
    Words, AcceptedLineTest,
    ::testing::Values(Spelling{"PLUG", Kind::Plug}, Spelling{"UNPLUG", Kind::Unplug},
                      Spelling{"AUTH", Kind::Auth}, Spelling{"READY", Kind::Ready},
                      Spelling{"SUSPEND", Kind::Suspend}, Spelling{"FAULT", Kind::Fault},
                      Spelling{"CLEAR", Kind::Clear}, Spelling{"STATE", Kind::State},
                      // nc and telnet send CRLF; spaces are noise
                      Spelling{"PLUG\r", Kind::Plug}, Spelling{"  READY  ", Kind::Ready},
                      Spelling{"CURRENT 16", Kind::Current},
                      Spelling{"CURRENT 7.5\r", Kind::Current},
                      Spelling{"CURRENT   0", Kind::Current}));

TEST(LineProtocolTest, CurrentCarriesItsValue) {
    EXPECT_DOUBLE_EQ(parseCommand("CURRENT 16").amperes, 16.0);
    EXPECT_DOUBLE_EQ(parseCommand("CURRENT 7.5").amperes, 7.5);
    EXPECT_DOUBLE_EQ(parseCommand("CURRENT 80").amperes, 80.0);
}

class RefusedLineTest : public ::testing::TestWithParam<const char*> {};

TEST_P(RefusedLineTest, IsInvalidWithAReason) {
    const Command command = parseCommand(GetParam());
    EXPECT_FALSE(command.valid());
    EXPECT_EQ(command.kind, Kind::Invalid);
    EXPECT_FALSE(command.reason.empty());
}

INSTANTIATE_TEST_SUITE_P(Lines, RefusedLineTest,
                         ::testing::Values("", "   ", "\r",  // empty
                                           "plug", "Plug",   // case matters
                                           "PLUGIN", "HELLO", "PLUG UNPLUG",
                                           "PLUG 1",   // no argument expected
                                           "CURRENT",  // argument missing
                                           "CURRENT abc", "CURRENT 16A", "CURRENT 1e400",
                                           "CURRENT -1", "CURRENT 81"));

TEST(LineProtocolTest, ReasonsNameTheProblem) {
    EXPECT_EQ(parseCommand("").reason, "empty line");
    EXPECT_EQ(parseCommand("HELLO").reason, "unknown command");
    EXPECT_EQ(parseCommand("CURRENT").reason, "CURRENT needs a value in amperes");
    EXPECT_EQ(parseCommand("CURRENT 99").reason, "CURRENT value out of range 0 to 80");
}

// ---------------------------------------------------------------------------
// SocketVehicle: signals first, then the event (what REQ-007 relies on).
// ---------------------------------------------------------------------------

TEST(SocketVehicleTest, StartsUnpluggedAndNotReady) {
    const SocketVehicle vehicle;
    EXPECT_FALSE(vehicle.isPluggedIn());
    EXPECT_FALSE(vehicle.isReady());
    EXPECT_DOUBLE_EQ(vehicle.requestedCurrentA(), 0.0);
}

TEST(SocketVehicleTest, PlugAndReadySetTheSignalsAndRaiseTheirEvents) {
    SocketVehicle vehicle;
    EXPECT_EQ(vehicle.apply(parseCommand("PLUG")), Event::PlugIn);
    EXPECT_TRUE(vehicle.isPluggedIn());
    EXPECT_FALSE(vehicle.isReady());

    EXPECT_EQ(vehicle.apply(parseCommand("READY")), Event::EvReady);
    EXPECT_TRUE(vehicle.isReady());
}

TEST(SocketVehicleTest, SuspendClearsReadyAndUnplugClearsBoth) {
    SocketVehicle vehicle;
    vehicle.apply(parseCommand("PLUG"));
    vehicle.apply(parseCommand("READY"));

    EXPECT_EQ(vehicle.apply(parseCommand("SUSPEND")), Event::EvSuspend);
    EXPECT_TRUE(vehicle.isPluggedIn());
    EXPECT_FALSE(vehicle.isReady());

    vehicle.apply(parseCommand("READY"));
    EXPECT_EQ(vehicle.apply(parseCommand("UNPLUG")), Event::Unplug);
    EXPECT_FALSE(vehicle.isPluggedIn());
    EXPECT_FALSE(vehicle.isReady());
}

TEST(SocketVehicleTest, OperatorCommandsRaiseEventsWithoutTouchingSignals) {
    SocketVehicle vehicle;
    EXPECT_EQ(vehicle.apply(parseCommand("AUTH")), Event::Authorize);
    EXPECT_EQ(vehicle.apply(parseCommand("FAULT")), Event::Fault);
    EXPECT_EQ(vehicle.apply(parseCommand("CLEAR")), Event::Clear);
    EXPECT_FALSE(vehicle.isPluggedIn());
    EXPECT_FALSE(vehicle.isReady());
}

TEST(SocketVehicleTest, CurrentStateAndInvalidRaiseNothing) {
    SocketVehicle vehicle;
    EXPECT_EQ(vehicle.apply(parseCommand("CURRENT 32")), std::nullopt);
    EXPECT_DOUBLE_EQ(vehicle.requestedCurrentA(), 32.0);
    EXPECT_EQ(vehicle.apply(parseCommand("STATE")), std::nullopt);
    EXPECT_EQ(vehicle.apply(parseCommand("garbage")), std::nullopt);
    EXPECT_DOUBLE_EQ(vehicle.requestedCurrentA(), 32.0);
}

}  // namespace
