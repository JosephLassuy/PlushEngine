# Tutorial: Drawing a 3D Cube

This folder contains a **minimal, heavily commented example** that draws a 3D cube using the PlushEngine graphics API, plus guides that assume **no prior graphics experience**.

## Contents

- **`main.cpp`** – Example application: creates a renderer, uploads cube geometry and shaders, and draws one frame per loop iteration. Every section is commented to explain what it does.

- **`UNDER_THE_HOOD_FOR_BEGINNERS.md`** – **Very new to graphics?** Plain-language intro (CPU/GPU, frames, vertices, shaders, buffers, pipeline), then **side-by-side**: the code you write (our API) and the real code that runs under the hood (snippets from `renderer.cpp` and `backend_vulkan.cpp`) for startup, buffers, shaders, frame loop, and drawing.

- **`UNDER_THE_HOOD.md`** – **Not familiar with our API yet?** Explains what “our API” is, where it lives in the code, and **exactly what code runs** when you call something (Renderer → VulkanBackend → SDL_GPU). Handles, descriptors, backend state, and one full example from `createBuffer` to the GPU.

- **`BEGINNER_BACKENDS_GUIDE.md`** – **Start here if you're new to graphics.** For each concept (window/device, buffers, shaders, pipeline, frame loop, drawing, cleanup) it shows:
  - **What you write** – our API with copy-paste examples
  - **How Vulkan handles it** – what happens under the hood when the backend is Vulkan
  - **How WebGPU would handle it** – same idea in the browser API
  - **How OpenGL would handle it** – same idea in the classic desktop API  
  Plus a quick-reference table at the end. No prior knowledge assumed.

- **`WALKTHROUGH.md`** – In-depth walkthrough of the same example and the Vulkan backend:
  - The example code step by step
  - The public graphics API (types, descriptors, resources, `Renderer`)
  - How the renderer forwards to the backend
  - The Vulkan backend in depth (SDL_GPU, device, command buffer, render pass, buffers, shaders, pipeline, draw calls)
  - End-to-end data flow from your code to the GPU

## Build and run

From the **project root**:

1. Compile shaders (once):  
   `zig build shaders`
2. Build:  
   `zig build`
3. Run the tutorial:  
   `zig build run-tutorial`

The tutorial loads shaders from `shaders/cube.vert.spv` and `shaders/cube.frag.spv`, so it must be run from the project root (or with the working directory set there).

## See also

- **docs/GRAPHICS_GUIDE.md** – High-level concepts and file layout for the graphics code.
- **UNDER_THE_HOOD_FOR_BEGINNERS.md** – Graphics basics + API vs under-the-hood code examples (startup, buffers, shaders, frame, draw).
- **UNDER_THE_HOOD.md** – What our API is and what runs under the hood when you call it (Renderer, backend, handles, state).
- **BEGINNER_BACKENDS_GUIDE.md** – Examples + how Vulkan, WebGPU, and OpenGL each handle the same operations (beginner-friendly).
