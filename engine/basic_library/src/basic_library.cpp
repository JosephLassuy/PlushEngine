#include "basic_library/basic_library.h"

namespace basic_library {

const char* Version() {
    return "0.1.0";
}

int Add(int a, int b) {
    return a + b;
}

int Subtract(int a, int b) {
    return a - b;
}

} // namespace basic_library