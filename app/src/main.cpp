#include <iostream>

#include "charger/Charger.h"

int main() {
    const charger::Charger charger;
    std::cout << "charger-daemon: " << charger.greeting() << '\n';
    return 0;
}