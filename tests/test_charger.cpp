#include <gtest/gtest.h>

#include "charger/Charger.h"

TEST(ChargerTest, GreetingIsHello) {
    const charger::Charger charger;
    EXPECT_EQ(charger.greeting(), "hello");
}