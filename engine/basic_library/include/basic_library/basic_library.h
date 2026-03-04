#pragma once

#include <iostream>
#include <string_view>
namespace basic_library {

template <typename... Ts> void Print(const Ts &...values) {
  ((std::cout << values << ' '), ...);
  std::cout << '\n';
}

const char *Version();
int Add(int a, int b);
int Subtract(int a, int b);

} // namespace basic_library
