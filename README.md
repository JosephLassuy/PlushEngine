## PlushEngine C++

A C++ project for PlushEngine, set up as a standalone CMake project.

### Layout

- `CMakeLists.txt` – CMake build configuration
- `vcpkg.json` – vcpkg manifest
- `src/main.cpp` – main entry point
- `engine/basic_library` – basic library and ECS components

### Prerequisites

- C++20 compiler (e.g. `gcc` or `clang`)
- CMake 3.21 or newer

### Configure and build

```bash
cmake -S . -B build
cmake --build build
```

### Run

```bash
./build/PlushEngine
```

