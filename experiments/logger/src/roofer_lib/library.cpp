#include <cstdint>
#include <iostream>

#include "library.h"

void hello() {
    std::cout << "Hello, World!" << std::endl;
}

std::uint32_t returns_42(std::uint32_t any_number) {
    return 42;
}
