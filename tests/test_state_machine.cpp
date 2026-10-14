#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <ostream>
#include <string>
#include <vector>

#include "charger/StateMachine.h"
#include "mocks/FakeClock.h"
#include "mocks/MockContactor.h"
#include "mocks/MockVehicle.h"

using charger::Event;
using charger::State;
using charger::StateMachine;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;
using namespace std::chrono_literals;

namespace charger {
// Lets GoogleTest print names instead of numbers when an assertion fails.
std::ostream& operator<<(std::ostream& os, State state) { return os << toString(state); }
std::ostream& operator<<(std::ostream& os, Event event) { return os << toString(event); }
}  // namespace charger

namespace {

enum class Contactor { Untouched, Closes, Opens };

struct Row {
    State from;
    Event event;
    State to;
    Contactor contactor;
};

// Readable test names and failure messages: "Charging + Fault -> Faulted".
void PrintTo(const Row& row, std::ostream* os) {
    *os << row.from << " + " << row.event << " -> " << row.to;
}

// The expected behaviour, transcribed from docs/design.md, NOT copied from the
// product's table. The test is only worth something if its expectations are
// written independently of the code they check.
// clang-format off
const std::vector<Row> kDesign = {
    {State::Available, Event::PlugIn, State::Preparing, Contactor::Untouched},
    {State::Preparing, Event::Authorize, State::SuspendedEV, Contactor::Untouched},
    {State::Preparing, Event::Unplug, State::Available, Contactor::Untouched},
    {State::SuspendedEV, Event::EvReady, State::Charging, Contactor::Closes},
    {State::SuspendedEV, Event::Authorize, State::Finishing, Contactor::Untouched},
    {State::SuspendedEV, Event::Unplug, State::Available, Contactor::Untouched},
    {State::Charging, Event::EvSuspend, State::SuspendedEV, Contactor::Opens},
    {State::Charging, Event::Authorize, State::Finishing, Contactor::Opens},
    {State::Charging, Event::Unplug, State::Available, Contactor::Opens},
    {State::Finishing, Event::Unplug, State::Available, Contactor::Untouched},
    {State::Available, Event::Fault, State::Faulted, Contactor::Opens},
    {State::Preparing, Event::Fault, State::Faulted, Contactor::Opens},
    {State::Charging, Event::Fault, State::Faulted, Contactor::Opens},
    {State::SuspendedEV, Event::Fault, State::Faulted, Contactor::Opens},
    {State::Finishing, Event::Fault, State::Faulted, Contactor::Opens},
    {State::Faulted, Event::Fault, State::Faulted, Contactor::Opens},
    {State::Faulted, Event::Clear, State::Available, Contactor::Untouched},
};
// clang-format on

const State kAllStates[] = {State::Available,   State::Preparing, State::Charging,
                            State::SuspendedEV, State::Finishing, State::Faulted};
const Event kAllEvents[] = {Event::PlugIn, Event::Authorize, Event::EvReady, Event::EvSuspend,
                            Event::Unplug, Event::Fault,     Event::Clear};

// Every (state, event) pair the design does not list. These must be ignored.
std::vector<Row> ignoredPairs() {
    std::vector<Row> pairs;
    for (const State state : kAllStates) {
        for (const Event event : kAllEvents) {
            bool listed = false;
            for (const Row& row : kDesign) {
                listed = listed || (row.from == state && row.event == event);
            }
            if (!listed) {
                pairs.push_back({state, event, state, Contactor::Untouched});
            }
        }
    }
    return pairs;
}

// Names each generated test after its row, e.g. "Charging_Fault".
std::string rowName(const ::testing::TestParamInfo<Row>& info) {
    return std::string(toString(info.param.from)) + "_" + toString(info.param.event);
}

// Shared setup. GoogleTest builds a fresh fixture for every single test, so no
// test can be affected by what another one did.
class StateMachineTest : public ::testing::Test {
protected:
    StateMachineTest() {
        // Default answers: a vehicle that is plugged in and ready.
        ON_CALL(vehicle_, isPluggedIn()).WillByDefault(Return(true));
        ON_CALL(vehicle_, isReady()).WillByDefault(Return(true));
    }

    // Reach a state the same way the product does: through events. There is no
    // setState() back door in the product code for tests to use.
    void driveTo(State target) {
        switch (target) {
            case State::Available:
                break;
            case State::Preparing:
                machine_.handle(Event::PlugIn);
                break;
            case State::SuspendedEV:
                driveTo(State::Preparing);
                machine_.handle(Event::Authorize);
                break;
            case State::Charging:
                driveTo(State::SuspendedEV);
                machine_.handle(Event::EvReady);
                break;
            case State::Finishing:
                driveTo(State::SuspendedEV);
                machine_.handle(Event::Authorize);
                break;
            case State::Faulted:
                machine_.handle(Event::Fault);
                break;
        }
        ASSERT_EQ(machine_.state(), target) << "driveTo() could not reach the state";
    }

    // NiceMock: calls we set no expectation on are allowed and ignored.
    NiceMock<MockVehicle> vehicle_;
    NiceMock<MockContactor> contactor_;
    FakeClock clock_;
    StateMachine machine_{vehicle_, contactor_, clock_};
};

// ---------------------------------------------------------------------------
// REQ-001..REQ-005: one generated test per row of the design table.
// ---------------------------------------------------------------------------
class TransitionTest : public StateMachineTest, public ::testing::WithParamInterface<Row> {};

TEST_P(TransitionTest, MatchesDesign) {
    const Row& row = GetParam();
    driveTo(row.from);

    // Expectations set here apply from now on, so the set-up above is excluded.
    EXPECT_CALL(contactor_, close()).Times(row.contactor == Contactor::Closes ? 1 : 0);
    EXPECT_CALL(contactor_, open()).Times(row.contactor == Contactor::Opens ? 1 : 0);
    EXPECT_TRUE(machine_.handle(row.event));
    EXPECT_EQ(machine_.state(), row.to);
}

INSTANTIATE_TEST_SUITE_P(DesignTable, TransitionTest, ::testing::ValuesIn(kDesign), rowName);

// ---------------------------------------------------------------------------
// REQ-001: every pair NOT in the table is ignored and leaves the contactor alone.
// ---------------------------------------------------------------------------
class IgnoredEventTest : public StateMachineTest, public ::testing::WithParamInterface<Row> {};

TEST_P(IgnoredEventTest, LeavesStateAndContactorAlone) {
    const Row& row = GetParam();
    driveTo(row.from);

    EXPECT_CALL(contactor_, close()).Times(0);
    EXPECT_CALL(contactor_, open()).Times(0);

    EXPECT_FALSE(machine_.handle(row.event));
    EXPECT_EQ(machine_.state(), row.from);
}

INSTANTIATE_TEST_SUITE_P(NotInTable, IgnoredEventTest, ::testing::ValuesIn(ignoredPairs()),
                         rowName);

// The two groups above must account for all 6 x 7 pairs, with no pair twice.
TEST(DesignTableTest, CoversEveryStateEventPairExactlyOnce) {
    EXPECT_EQ(kDesign.size() + ignoredPairs().size(),
              std::size(kAllStates) * std::size(kAllEvents));
    for (std::size_t i = 0; i < kDesign.size(); ++i) {
        for (std::size_t j = i + 1; j < kDesign.size(); ++j) {
            EXPECT_FALSE(kDesign[i].from == kDesign[j].from && kDesign[i].event == kDesign[j].event)
                << "duplicate row for " << kDesign[i].from << " + " << kDesign[i].event;
        }
    }
}

// Behaviour across a whole session.
// ---------------------------------------------------------------------------

// REQ-002, REQ-003: close on entering Charging, open on leaving, in that order.
TEST_F(StateMachineTest, SessionClosesThenOpensContactorExactlyOnce) {
    {
        InSequence inOrder;
        EXPECT_CALL(contactor_, close()).Times(1);
        EXPECT_CALL(contactor_, open()).Times(1);
    }
    for (const Event event :
         {Event::PlugIn, Event::Authorize, Event::EvReady, Event::Authorize, Event::Unplug}) {
        EXPECT_TRUE(machine_.handle(event));
    }
    EXPECT_EQ(machine_.state(), State::Available);
}

// REQ-005: nothing closes the contactor while Faulted; only Clear leaves.
TEST_F(StateMachineTest, NeverClosesContactorWhileFaulted) {
    driveTo(State::Faulted);
    EXPECT_CALL(contactor_, close()).Times(0);

    for (const Event event : kAllEvents) {
        if (event != Event::Clear) {
            machine_.handle(event);
            EXPECT_EQ(machine_.state(), State::Faulted) << "left Faulted on " << event;
        }
    }
}

// REQ-007: the hazard from Monday's safety notes, contactor closed with no
// vehicle. An EvReady event alone must not energise the cable.
TEST_F(StateMachineTest, RefusesToCloseWhenVehicleIsNotPluggedIn) {
    driveTo(State::SuspendedEV);
    ON_CALL(vehicle_, isPluggedIn()).WillByDefault(Return(false));
    EXPECT_CALL(contactor_, close()).Times(0);
    EXPECT_CALL(contactor_, open()).Times(1);

    machine_.handle(Event::EvReady);
    EXPECT_EQ(machine_.state(), State::Faulted);
}

TEST_F(StateMachineTest, RefusesToCloseWhenVehicleIsNotReady) {
    driveTo(State::SuspendedEV);
    ON_CALL(vehicle_, isReady()).WillByDefault(Return(false));
    EXPECT_CALL(contactor_, close()).Times(0);
    EXPECT_CALL(contactor_, open()).Times(1);

    machine_.handle(Event::EvReady);
    EXPECT_EQ(machine_.state(), State::Faulted);
}

// REQ-006: the Preparing timeout, tested at its boundary.
// ---------------------------------------------------------------------------
TEST_F(StateMachineTest, PreparingDoesNotTimeOutBefore60Seconds) {
    driveTo(State::Preparing);
    clock_.advance(StateMachine::kPreparingTimeout - 1ms);
    machine_.tick();
    EXPECT_EQ(machine_.state(), State::Preparing);
}

TEST_F(StateMachineTest, PreparingTimesOutToAvailableAt60Seconds) {
    driveTo(State::Preparing);
    clock_.advance(StateMachine::kPreparingTimeout);
    machine_.tick();
    EXPECT_EQ(machine_.state(), State::Available);
}
TEST_F(StateMachineTest, TimeoutAppliesOnlyToPreparing) {
    driveTo(State::SuspendedEV);
    clock_.advance(24h);
    machine_.tick();
    EXPECT_EQ(machine_.state(), State::SuspendedEV);
}

}  // namespace