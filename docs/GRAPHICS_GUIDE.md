# Graphics API Guide (Beginner-Friendly)

This guide explains how the PlushEngine graphics code is organized and what each concept means, assuming you're new to graphics programming.

---

## What Does the Graphics Code Do?

At a high level it does three things:

1. **Opens a window** – so you have something to draw into.
2. **Talks to the GPU** – the graphics card that actually draws pixels.
3. **Draws 3D (or 2D) stuff** – by sending geometry (e.g. a cube) and shaders (small programs that run on the GPU) so the GPU can turn them into a picture.

Everything in `graphic/` is a thin, backend-agnostic layer: your game code uses one API (the `gpu::` namespace), and we can plug in different “backends” (Vulkan, WebGPU, etc.) without changing the game.

---

## Big Idea: You Record Commands, the GPU Runs Them Later

The CPU (your C++ code) and the GPU (the graphics card) run in parallel. You don’t draw “right now” in the sense of the GPU finishing the work before the next line runs. Instead you:

1. **Record** a list of commands (clear the screen, use this shader, draw these triangles).
2. **Submit** that list to the GPU.
3. The GPU **executes** the list while your program can already be preparing the next frame.

So “draw a cube” in code means: add “draw this many triangles from these buffers with this pipeline” to the current command list, then submit the list. The actual pixel drawing happens asynchronously on the GPU.

---

## Main Concepts (In Order of Use)

### 1. Renderer

- **What:** The main object. It owns the window and the connection to the GPU (the “device”).
- **You use it to:** Create buffers, shaders, and pipelines; start/end frames and render passes; set pipeline and buffers and issue draw calls.
- **File:** `graphic/renderer.h` (declaration), `graphic/renderer.cpp` (implementation + window/event handling).

### 2. Frame

- **What:** One image that will be shown on screen. You typically do “one frame per loop iteration.”
- **Flow:** `beginFrame()` → do work (render pass, draws) → `endFrame()`. After `endFrame()` the commands are submitted; the GPU will present this frame to the window when it’s done.
- **Why:** The driver needs a clear boundary so it can hand you a new “command list” and swap the previous one to the GPU.

### 3. Render pass

- **What:** A block of rendering that draws into one or more “render targets” (usually the screen, i.e. the swapchain image). All draw calls that contribute to that image happen inside one render pass.
- **Flow:** `beginRenderPass(desc)` → set pipeline, buffers, uniforms, call `drawIndexed()` (and maybe more draws) → `endRenderPass()`.
- **Why:** GPUs are optimized for “start a pass (attach render targets), do many draws, end pass.” Our API reflects that.

### 4. Buffer

- **What:** A chunk of GPU memory. Typical uses: vertex data (positions, colors), index data (which vertices form triangles), or uniform data (e.g. a matrix for the camera).
- **You:** `createBuffer(desc)` with size and usage (e.g. `BufferUsage::Vertex`), then `writeBuffer(buffer, data, size)` to upload CPU data. Later you bind it and draw.
- **File:** Described in `graphic/descriptors.h` (BufferDesc) and `graphic/resources.h` (Buffer).

### 5. Shader

- **What:** A small program that runs on the GPU. We use two per draw: a **vertex shader** (runs per vertex; e.g. multiplies position by a matrix) and a **fragment shader** (runs per pixel; e.g. outputs a color).
- **Format:** Shaders are compiled to SPIR-V (a binary format). We write them in GLSL (e.g. `shaders/cube.vert` / `cube.frag`) and compile with `glslangValidator` (`zig build shaders`).
- **You:** `createShader(desc)` with SPIR-V bytes and stage (vertex/fragment), then attach two shaders to a pipeline.
- **File:** Shader types in `graphic/descriptors.h` and `graphic/resources.h`; creation is in the backend.

### 6. Graphics pipeline

- **What:** A fixed combination of: vertex shader, fragment shader, vertex layout (how to read the vertex buffer), primitive type (triangles, etc.), and fixed render state (culling, blending, etc.). You create it once and bind it before drawing.
- **You:** `createGraphicsPipeline(desc)` with the two shaders and a `VertexLayoutDesc` (stride + attributes). Then in the render pass you `setPipeline(pipeline)` and `drawIndexed(...)`.
- **Why:** The GPU is fastest when big batches of state (shaders + layout + state) are changed rarely and many triangles are drawn with the same pipeline.

### 7. Draw call

- **What:** “Draw this many indices from the bound vertex and index buffers, using the bound pipeline and the last pushed uniforms.”
- **You:** Between `beginRenderPass` and `endRenderPass`: `setPipeline(pipeline)`, `pushVertexUniform(0, &mvp, sizeof(mvp))`, `setVertexBuffer(0, vb, 0)`, `setIndexBuffer(ib, 0)`, then `drawIndexed(36)` (for 36 indices = 12 triangles).
- **Why:** This is the minimal set of binds the GPU needs to actually rasterize your geometry.

### 8. Backend

- **What:** The implementation that translates our API into a real API (Vulkan via SDL_GPU, or later WebGPU, etc.). The rest of the engine only sees `gpu::` types.
- **File:** `graphic/detail/backend_vulkan.h` and `graphic/backend_vulkan.cpp`. The renderer holds a `VulkanBackend` and forwards all work to it when the backend is Vulkan.

---

## How the Files Are Organized

```
engine/basic_library/
├── include/
│   ├── graphic.h              ← Single include for the whole API (include this in apps)
│   └── graphic/
│       ├── types.h             ← Enums (Backend, Format, BufferUsage, …), Extent2D, Color, VertexFormat
│       ├── descriptors.h       ← “Description” structs (BufferDesc, ShaderDesc, VertexLayoutDesc, …)
│       ├── resources.h         ← Opaque handles (Buffer, Shader, GraphicsPipeline) + GraphicsPipelineDesc
│       ├── renderer.h          ← Renderer class (the main entry point)
│       └── detail/
│           └── backend_vulkan.h   ← Vulkan backend class (internal use)
└── src/
    └── graphic/
        ├── renderer.cpp        ← Renderer implementation: window, events, dispatch to backend
        └── backend_vulkan.cpp  ← Vulkan implementation: device, buffers, shaders, pipelines, draws
```

- **types.h** – No dependencies on other graphic headers. Use it if you only need enums and small types.
- **descriptors.h** – Describes “how to create” things (sizes, formats, strides). No GPU handles.
- **resources.h** – The actual handle types (Buffer, Shader, GraphicsPipeline) and pipeline creation desc.
- **renderer.h** – The only class the app uses; it creates and uses buffers, shaders, pipelines, and issues draws.
- **graphic.h** – One-line include so existing code can keep `#include <graphic.h>` and get the full API.

The **Vulkan backend** lives in `backend_vulkan.cpp`: it holds the SDL_GPU device, command buffer, render pass, and current bindings, and implements every “create/write/release” and “set/draw” in terms of SDL_GPU (Vulkan). The **renderer** in `renderer.cpp` owns the window and the backend; it never touches Vulkan or SDL_GPU types in its public API.

---

## Typical Order of Operations

1. **Startup**
   - Create a `gpu::Renderer`, fill a `RendererDesc` (backend, window title/size).
   - Call `renderer.initialize(desc)` → opens window and creates the GPU device (e.g. Vulkan).

2. **Create resources (once)**
   - Load shader bytes (e.g. from `shaders/cube.vert.spv` / `cube.frag.spv`).
   - `createBuffer` for vertex and index data; `writeBuffer` to upload the cube.
   - `createShader` for vertex and fragment; `createGraphicsPipeline` with those shaders and the vertex layout.

3. **Every frame**
   - `pollEvents()` (and exit if the user closed the window).
   - `beginFrame()`.
   - `beginRenderPass(pass_desc)` (clear color, etc.).
   - Compute camera (e.g. view-projection matrix); `pushVertexUniform(0, &mvp, sizeof(mvp))`.
   - `setPipeline(pipeline)`, `setVertexBuffer(0, vb, 0)`, `setIndexBuffer(ib, 0)`.
   - `drawIndexed(index_count)`.
   - `endRenderPass()`, then `endFrame()`.

4. **Shutdown**
   - Release pipelines, shaders, buffers (or let the renderer go out of scope after shutdown).
   - `renderer.shutdown()` (or rely on destructor).

---

## Glossary

- **Backend** – Implementation of the graphics API (Vulkan, WebGPU, …). We only have Vulkan (via SDL_GPU) for now.
- **Buffer** – GPU memory used for vertices, indices, uniforms, etc.
- **Command buffer** – The list of GPU commands you’re building (acquired in `beginFrame`, submitted in `endFrame`).
- **Descriptor** – A struct that *describes* how to create something (e.g. BufferDesc), not the GPU object itself.
- **Draw call** – One call that tells the GPU to draw a number of primitives (e.g. triangles) with the currently bound state.
- **Fragment shader** – GPU program that runs per pixel and outputs color (and optionally depth).
- **Pipeline** – The combined vertex + fragment shaders plus vertex layout and fixed state; bound before drawing.
- **Render pass** – Section of a frame where you render into specific render targets (e.g. the screen).
- **SPIR-V** – Binary shader format used by Vulkan (and our backend). We compile GLSL to SPIR-V with `glslangValidator`.
- **Swapchain** – The set of images that are presented to the window; we get one image per frame and draw into it.
- **Uniform** – Data (e.g. a matrix) you pass from CPU to shader; we use “push” style (pushVertexUniform) per draw.
- **Vertex shader** – GPU program that runs per vertex; transforms positions and passes data to the fragment shader.
- **Vertex layout** – Describes the format of the vertex buffer (stride, attributes: location, format, offset).

---

## Summary

- **Renderer** = window + GPU device; you create buffers, shaders, pipelines through it and record draws each frame.
- **Frame** = one screen image; you **begin frame** → **begin render pass** → **set pipeline + buffers + uniform** → **draw indexed** → **end render pass** → **end frame**.
- **Buffers** hold geometry (and indices); **shaders** run on the GPU; **pipeline** ties shaders and vertex layout together; **draw call** uses the current pipeline and buffers to draw triangles.
- Code is split into **types**, **descriptors**, **resources**, and **renderer** in headers, and **renderer.cpp** (window + dispatch) plus **backend_vulkan.cpp** (all Vulkan/SDL_GPU work) in source, so it’s easier to find things and add more backends later.
