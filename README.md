## PlushEngine C++

A C++ project for PlushEngine, set up with a Zig build script.

The main app now boots through a small engine-owned renderer API that follows a
WebGPU-style shape while using `Vulkan` as the first desktop backend. This
keeps the public rendering surface portable for a future web target without
exposing Vulkan details in app code.

### Layout

- `build.zig` - Zig build configuration
- `vcpkg.json` - vcpkg manifest
- `src/main.cpp` - main entry point (cube demo)
- `example/flappybird/main.cpp` - Flappybird example app
- `engine/basic_library` - basic library and ECS components
- `engine/basic_library/include/graphic.h` - single include for the graphics API
- `engine/basic_library/include/graphic/` - API split into `types.h`, `descriptors.h`, `resources.h`, `renderer.h`
- `engine/basic_library/src/graphic/` - `renderer.cpp` (window + dispatch), `backend_vulkan.cpp` (Vulkan implementation)
- `docs/GRAPHICS_GUIDE.md` - beginner-friendly explanation of the graphics API and concepts

### Prerequisites

- Zig 0.15 or newer
- SDL3 development files discoverable via `pkg-config`
- Vulkan loader/development files discoverable via `pkg-config`

### Build

```bash
zig build
```

### Run main app

```bash
zig build run
```

This opens an `SDL3` window using the engine renderer layer and clears the
swapchain through the Vulkan backend.

### Run Flappybird

```bash
zig build run-flappybird
```

