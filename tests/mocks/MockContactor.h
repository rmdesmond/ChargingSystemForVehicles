#pragma once

#include <gmock/gmock.h>

#include "charger/ContactorInterface.h"

class MockContactor : public charger::ContactorInterface {
public:
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(void, open, (), (override));
    MOCK_METHOD(bool, isClosed, (), (const, override));
};