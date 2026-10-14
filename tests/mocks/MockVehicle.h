#pragma once

#include <gmock/gmock.h>

#include "charger/VehicleInterface.h"

// MOCK_METHOD(return type, name, (arguments), (qualifiers));
class MockVehicle : public charger::VehicleInterface {
public:
    MOCK_METHOD(bool, isPluggedIn, (), (const, override));
    MOCK_METHOD(double, requestedCurrentA, (), (const, override));
    MOCK_METHOD(bool, isReady, (), (const, override));
};