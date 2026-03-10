# In-Depth Tutorial Walkthrough

This document walks through the **tutorial example** (a 3D rotating cube) and then dives into **every layer** of the graphics code: the public API, the renderer, and the Vulkan backend. By the end you’ll understand what each call does and how it maps to real GPU work.

---

## Table of Contents

1. [Overview: What We’re Building](#toc-1-overview)
2. [Part I: The Example Code (tutorial/main.cpp)](#toc-2-part-i-example-code)
   - [Step 1: Create and Initialize the Renderer](#toc-21-step-1-renderer)
   - [Step 2: Create GPU Resources](#toc-22-step-2-resources)
   - [Step 3: Main Loop](#toc-23-step-3-main-loop)
   - [Step 4: Cleanup](#toc-24-step-4-cleanup)
3. [Part II: The Public Graphics API](#toc-3-part-ii-api)
   - [Types](#toc-31-types)
   - [Descriptors](#toc-32-descriptors)
   - [Resources](#toc-33-resources)
   - [Renderer](#toc-34-renderer)
4. [Part III: From API to Backend (renderer.cpp)](#toc-4-part-iii-backend)
5. [Part IV: The Vulkan Backend in Depth](#toc-5-part-iv-vulkan)
   - [Backend State](#toc-51-backend-state)
   - [Device and Window](#toc-52-device-window)
   - [Frame and Command Buffer](#toc-53-frame-command-buffer)
   - [Render Pass](#toc-54-render-pass)
   - [Buffers](#toc-55-buffers)
   - [Shaders](#toc-56-shaders)
   - [Graphics Pipeline](#toc-57-graphics-pipeline)
   - [Recording Draws](#toc-58-recording-draws)
6. [Part V: End-to-End Data Flow](#toc-6-part-v-data-flow)
   - [One Frame, Top to Bottom](#toc-61-one-frame)
   - [Where the Data Lives](#toc-62-where-data-lives)
   - [Summary Table](#toc-63-summary-table)
7. [Building and Running the Tutorial](#toc-building-running)

---

<a id="toc-1-overview"></a>
## 1. Overview: What We’re Building

We build a **desktop application** that:

- Opens a **window** (via SDL).
- Uses **Vulkan** (through SDL_GPU) to talk to the GPU.
- Each frame: **clears** the window to a color, **draws a 3D cube** with a camera (view + projection), then **presents** the image.

The code is split into:

| Layer | Where | Role |
|-------|--------|------|
| **Your code** | `tutorial/main.cpp` (or `src/main.cpp`) | Creates renderer, resources, and the frame loop. |
| **Public API** | `graphic/renderer.h`, `resources.h`, `descriptors.h`, `types.h` | `gpu::Renderer` and all handles/descriptors. |
| **Renderer** | `graphic/renderer.cpp` | Owns window + backend; forwards every call to the backend. |
| **Vulkan backend** | `graphic/backend_vulkan.cpp`, `detail/backend_vulkan.h` | Implements the API using SDL_GPU (Vulkan). |

You never touch Vulkan or SDL directly in the example; everything goes through `gpu::Renderer`.

---

<a id="toc-2-part-i-example-code"></a>
## 2. Part I: The Example Code (tutorial/main.cpp)

The example does four things: **create the renderer**, **create GPU resources**, **run the main loop**, and **clean up**.

<a id="toc-21-step-1-renderer"></a>
### 2.1 Step 1: Create and Initialize the Renderer

```cpp
gpu::Renderer renderer;
gpu::RendererDesc renderer_desc{};
renderer_desc.device.backend = gpu::Backend::Vulkan;
renderer_desc.surface.window.title = "Tutorial – Cube";
renderer_desc.surface.window.size = {1280, 720};
if (!renderer.initialize(renderer_desc)) { ... }
```

- **`Renderer`** is the single object that owns the window and the GPU “device.”
- **`RendererDesc`** tells it: use the **Vulkan** backend, and create a window with the given title and size.
- **`initialize()`** does (under the hood):
  - SDL init and window creation (`SDL_CreateWindow`).
  - For Vulkan: create an SDL_GPU device (`SDL_CreateGPUDevice(..., "vulkan")`) and claim the window for that device (`SDL_ClaimWindowForGPUDevice`). That sets up the Vulkan instance, physical device, logical device, and swapchain for the window.

So after this, you have a window and a Vulkan device ready to record commands.

<a id="toc-22-step-2-resources"></a>
### 2.2 Step 2: Create GPU Resources

We create **buffers**, **shaders**, and a **graphics pipeline** once and reuse them every frame.

#### 2.2.1 Load shader bytecode (SPIR-V)

```cpp
std::vector<std::uint8_t> vertSpv = loadFile("shaders/cube.vert.spv");
std::vector<std::uint8_t> fragSpv = loadFile("shaders/cube.frag.spv");
```

Shaders are written in **GLSL** (`shaders/cube.vert`, `shaders/cube.frag`) and compiled to **SPIR-V** with `zig build shaders` (which runs `glslangValidator`). The engine only sees the compiled bytes.

#### 2.2.2 Vertex buffer

```cpp
gpu::BufferDesc vbDesc{};
vbDesc.size = sizeof(cubeVertices);
vbDesc.usage = gpu::BufferUsage::Vertex;
vertexBuffer = renderer.createBuffer(vbDesc);
renderer.writeBuffer(vertexBuffer, cubeVertices, sizeof(cubeVertices));
```

- **`createBuffer`** allocates a chunk of GPU memory with the given size and usage (here: vertex data).
- **`writeBuffer`** uploads CPU data (the 8 cube vertices: position + color each) into that buffer. Under the hood this uses a **staging/transfer buffer** and a **copy command** so the data is in GPU-visible memory for the draw.

#### 2.2.3 Index buffer

```cpp
gpu::BufferDesc ibDesc{};
ibDesc.size = sizeof(cubeIndices);
ibDesc.usage = gpu::BufferUsage::Index;
indexBuffer = renderer.createBuffer(ibDesc);
renderer.writeBuffer(indexBuffer, cubeIndices, sizeof(cubeIndices));
```

Same idea: GPU buffer sized for 36 indices, then upload the index list that defines the 12 triangles of the cube.

#### 2.2.4 Shaders

```cpp
gpu::ShaderDesc vsDesc{};
vsDesc.stage = gpu::ShaderStage::Vertex;
vsDesc.code = vertSpv.data();
vsDesc.code_size = vertSpv.size();
vsDesc.entry_point = "main";
vsDesc.num_uniform_buffers = 1;  // MVP matrix at set=1 binding=0
vertShader = renderer.createShader(vsDesc);
// ... similarly fragShader with ShaderStage::Fragment
```

- **Vertex shader**: runs per vertex; we tell the API it has **one uniform buffer** (the MVP matrix). That matches the GLSL: `layout(set = 1, binding = 0) uniform Uniforms { mat4 uMVP; };`
- **Fragment shader**: runs per pixel; no uniforms in this example.

The backend compiles/creates a **VkShaderModule** (via SDL_GPU) for each and keeps the handle.

#### 2.2.5 Vertex layout

```cpp
gpu::VertexAttributeDesc attrs[2] = {
    {0, gpu::VertexFormat::Float3, 0},   // location 0: position (first 12 bytes)
    {1, gpu::VertexFormat::Float3, 12}, // location 1: color (next 12 bytes)
};
gpu::VertexLayoutDesc layout{};
layout.stride = sizeof(Vertex);  // 24 bytes per vertex
layout.attribute_count = 2;
layout.attributes = attrs;
```

This describes **how** to read the vertex buffer: each vertex is 24 bytes; attribute at location 0 is 3 floats at offset 0 (position), attribute at location 1 is 3 floats at offset 12 (color). This must match the vertex shader `layout(location=0) in vec3 aPos` and `layout(location=1) in vec3 aColor`.

#### 2.2.6 Graphics pipeline

```cpp
gpu::GraphicsPipelineDesc pipeDesc{};
pipeDesc.vertex_shader = vertShader;
pipeDesc.fragment_shader = fragShader;
pipeDesc.vertex_layout = layout;
pipeDesc.primitive_type = gpu::PrimitiveTopology::TriangleList;
pipeline = renderer.createGraphicsPipeline(pipeDesc);
```

The **pipeline** is the full fixed state for one way of drawing: the two shaders, the vertex layout, triangle list, and (in the backend) rasterizer, blend, depth state, etc. We create it once and bind it before drawing.

<a id="toc-23-step-3-main-loop"></a>
### 2.3 Step 3: Main Loop (One Frame per Iteration)

```cpp
gpu::RenderPassDesc pass_desc{};
pass_desc.color_attachment.clear_color = {0.12f, 0.18f, 0.32f, 1.0f};
Mat4 view = lookAt(1.2f, 1.0f, 1.8f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

while (renderer.pollEvents()) {
    if (!renderer.beginFrame()) break;
    if (!renderer.beginRenderPass(pass_desc)) break;

    if (haveCube) {
        gpu::Extent2D size = renderer.getSurfaceSize();
        float aspect = (size.height > 0) ? (float)size.width / (float)size.height : 1280.0f/720.0f;
        Mat4 proj = perspective(60.0f, aspect, 0.1f, 100.0f);
        Mat4 mvp;
        mat4Mul(&mvp, &proj, &view);

        renderer.setPipeline(pipeline);
        renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));
        renderer.setVertexBuffer(0, vertexBuffer, 0);
        renderer.setIndexBuffer(indexBuffer, 0);
        renderer.drawIndexed(36);
    }

    renderer.endRenderPass();
    if (!renderer.endFrame()) break;
}
```

- **`pollEvents()`**: processes SDL events (e.g. quit); returns false when the app should exit.
- **`beginFrame()`**: acquires a **command buffer** for this frame. All subsequent commands are recorded into it.
- **`beginRenderPass(pass_desc)`**:  
  - Waits for and acquires the **swapchain image** (the texture we’ll draw into).  
  - Starts a **render pass** that renders into that image and **clears** it to `clear_color`.
- **Inside the pass**:  
  - **`getSurfaceSize()`**: current window size in pixels (for aspect ratio).  
  - Build **projection × view** and upload it as the **vertex uniform** (slot 0).  
  - **`setPipeline`** / **`setVertexBuffer`** / **`setIndexBuffer`**: bind pipeline and buffers for the draw.  
  - **`drawIndexed(36)`**: record “draw 36 indices (12 triangles) using the current pipeline, vertex buffer, index buffer, and pushed uniform.”
- **`endRenderPass()`**: end the render pass (no more draws into this target).
- **`endFrame()`**: **submit** the command buffer to the GPU and prepare for present. The GPU will execute the commands and then present the swapchain image to the window.

So: **frame → pass → set state → draw → end pass → submit**. The math (MVP, aspect) is just CPU work to fill the uniform; the actual 3D transform happens in the vertex shader on the GPU.

<a id="toc-24-step-4-cleanup"></a>
### 2.4 Step 4: Cleanup

```cpp
if (pipeline.valid()) renderer.releaseGraphicsPipeline(pipeline);
if (fragShader.valid()) renderer.releaseShader(fragShader);
if (vertShader.valid()) renderer.releaseShader(vertShader);
if (indexBuffer.valid()) renderer.releaseBuffer(indexBuffer);
if (vertexBuffer.valid()) renderer.releaseBuffer(vertexBuffer);
```

Release resources in reverse order of creation (pipelines reference shaders and buffers; we don’t need to strict order for this simple case, but it’s good practice). The renderer destructor (or `shutdown()`) then destroys the backend and the window.

---

<a id="toc-3-part-ii-api"></a>
## 3. Part II: The Public Graphics API

All types and the main class live under the **`gpu`** namespace and are included via **`graphic.h`** (or the split headers).

<a id="toc-31-types"></a>
### 3.1 Types (`graphic/types.h`)

- **`Backend`**: Vulkan, WebGPU, OpenGL (only Vulkan is implemented).
- **`Format`**: pixel formats (e.g. BGRA8_UNorm for the swapchain).
- **`BufferUsage`**: bitmask (Vertex, Index, Uniform, …).
- **`PrimitiveTopology`**: TriangleList, TriangleStrip.
- **`ShaderStage`**: Vertex, Fragment.
- **`Extent2D`**: width, height.
- **`Color`**: r, g, b, a (0–1 float).
- **`VertexFormat`**: Float3, Float4 (per-attribute format).

<a id="toc-32-descriptors"></a>
### 3.2 Descriptors (`graphic/descriptors.h`)

**Creation/config structs** (no GPU state, just data to describe what we want):

- **`DeviceDesc`**: backend, validation.
- **`WindowDesc`**: title, size, resizable, high DPI.
- **`SurfaceDesc`**: wraps window.
- **`RendererDesc`**: device + surface.
- **`RenderPassDesc`** / **`RenderPassColorAttachmentDesc`**: clear color, format.
- **`BufferDesc`**: size, usage.
- **`ShaderDesc`**: stage, SPIR-V code, code_size, entry_point, num_uniform_buffers.
- **`VertexAttributeDesc`**: location, format, offset.
- **`VertexLayoutDesc`**: stride, attribute_count, attributes.

<a id="toc-33-resources"></a>
### 3.3 Resources (`graphic/resources.h`)

**Handles** (opaque to the user; the backend stores the real object):

- **`Buffer`**: `void* handle`; created with `createBuffer`, filled with `writeBuffer`, released with `releaseBuffer`.
- **`Shader`**: `void* handle`; created with `createShader`, released with `releaseShader`.
- **`GraphicsPipeline`**: `void* handle`; created with `createGraphicsPipeline`, released with `releaseGraphicsPipeline`. **`GraphicsPipelineDesc`** holds vertex/fragment shaders, vertex layout, and primitive type.

<a id="toc-34-renderer"></a>
### 3.4 Renderer (`graphic/renderer.h`)

The **`Renderer`** class is the only object the app uses for graphics:

- **Lifecycle**: `initialize(RendererDesc)`, `shutdown()`, `pollEvents()`, `shouldClose()`.
- **Frame**: `beginFrame()`, `beginRenderPass(RenderPassDesc)`, `endRenderPass()`, `endFrame()`.
- **Queries**: `backendName()`, `getSurfaceSize()`.
- **Buffers**: `createBuffer`, `writeBuffer`, `releaseBuffer`.
- **Shaders**: `createShader`, `releaseShader`.
- **Pipeline**: `createGraphicsPipeline`, `releaseGraphicsPipeline`.
- **Drawing**: `setPipeline`, `pushVertexUniform`, `setVertexBuffer`, `setIndexBuffer`, `drawIndexed`.

So the **tutorial code only needs** these types and the `Renderer`; it never sees Vulkan or SDL.

---

<a id="toc-4-part-iii-backend"></a>
## 4. Part III: From API to Backend (renderer.cpp)

The **Renderer** owns the window and a **backend** implementation. For Vulkan it holds a **`std::unique_ptr<VulkanBackend>`**.

### 4.1 Structure

- **`Renderer::Impl`** holds:
  - `Backend backend`
  - `SDL_Window* window`
  - `std::unique_ptr<VulkanBackend> vulkan`
  - SDL init and “should close” flags.

- **`initialize(RendererDesc)`**:
  - Initializes SDL, creates the window.
  - If `desc.device.backend == Vulkan`, creates `VulkanBackend` and calls `vulkan->init(window, desc)`. That creates the SDL_GPU device and claims the window. Other backends are reported as “not implemented.”

- **Every other method** follows the same pattern:
  - If the backend is Vulkan and `vulkan` is set, **forward** to `impl_->vulkan->...`.
  - Example: `beginFrame()` → `impl_->vulkan->beginFrame()`; `createBuffer(desc)` → `impl_->vulkan->createBuffer(desc)`; `drawIndexed(36)` → `impl_->vulkan->drawIndexed(36)`.

So **renderer.cpp** is a thin **dispatcher**: it knows about the window and events (SDL) and delegates all GPU work to the backend. The backend interface mirrors the public draw/resource API.

### 4.2 Why This Design?

- The **same tutorial code** can later work with a WebGPU or OpenGL backend by just changing `RendererDesc.device.backend` and implementing that backend.
- All Vulkan/SDL_GPU details are **hidden** in `VulkanBackend`; the rest of the engine only sees `gpu::` types.

---

<a id="toc-5-part-iv-vulkan"></a>
## 5. Part IV: The Vulkan Backend in Depth

The Vulkan backend is implemented in **`graphic/backend_vulkan.cpp`** and declared in **`graphic/detail/backend_vulkan.h`**. It uses **SDL_GPU**, which wraps Vulkan concepts (device, command buffer, render pass, buffers, shaders, pipelines) in a simpler C API.

<a id="toc-51-backend-state"></a>
### 5.1 Backend State (`VulkanBackend::State`)

The backend keeps a **`State`** struct (unique_ptr) holding:

- **`SDL_Window* window`**: the window we render into.
- **`SDL_GPUDevice* device`**: the GPU “device” (Vulkan device + queue, etc.).
- **`SDL_GPUCommandBuffer* command_buffer`**: the current frame’s command list (acquired in `beginFrame`, submitted in `endFrame`).
- **`SDL_GPURenderPass* render_pass`**: the active render pass (started in `beginRenderPass`, ended in `endRenderPass`).
- **`SDL_GPUGraphicsPipeline* current_pipeline`**: pipeline set by the last `setPipeline`.
- **`SDL_GPUBuffer* current_vertex_buffer`** and **`current_vertex_offset`**: vertex buffer binding.
- **`SDL_GPUBuffer* current_index_buffer`** and **`current_index_offset`**: index buffer binding.

So the backend **remembers** the current pipeline and buffer bindings and applies them at **draw** time.

<a id="toc-52-device-window"></a>
### 5.2 Device and Window (`init`)

```cpp
state_->device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV,
    desc.device.enable_validation,
    "vulkan"
);
SDL_ClaimWindowForGPUDevice(state_->device, state_->window);
```

- **`SDL_CreateGPUDevice`**: creates a Vulkan instance, physical device, logical device, and prefers SPIR-V shaders. The `"vulkan"` driver name selects the Vulkan backend.
- **`SDL_ClaimWindowForGPUDevice`**: ties the SDL window to this device so SDL_GPU can create a swapchain and present to the window. Under the hood that means creating a VkSurfaceKHR from the window and a VkSwapchainKHR.

So after `init`, we have a **Vulkan device** and a **swapchain** for the window.

<a id="toc-53-frame-command-buffer"></a>
### 5.3 Frame and Command Buffer (`beginFrame` / `endFrame`)

- **`beginFrame()`**:  
  - If we don’t already have a command buffer, **acquire** one: `state_->command_buffer = SDL_AcquireGPUCommandBuffer(state_->device)`.  
  - In Vulkan terms, this gives you a **VkCommandBuffer** from a pool, ready for recording.

- **`endFrame()`**:  
  - First **end the render pass** if it’s still open (so we don’t leave it dangling).  
  - **Submit** the command buffer: `SDL_SubmitGPUCommandBuffer(state_->command_buffer)`.  
  - That queues the command buffer on the GPU queue; the GPU will execute it and then present the swapchain image.  
  - We then clear `state_->command_buffer` so the next frame gets a fresh one.

So the **lifecycle** is: acquire command buffer → record (render pass + draws) → submit. No drawing is done outside a render pass.

<a id="toc-54-render-pass"></a>
### 5.4 Render Pass (`beginRenderPass` / `endRenderPass`)

- **`beginRenderPass(RenderPassDesc)`**:  
  1. **Acquire the swapchain image**: `SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, ...)`. This may block until the next image is available (vsync / present). We get an **SDL_GPUTexture*** that is the current back buffer.  
  2. Fill **SDL_GPUColorTargetInfo**: texture = swapchain texture, clear color from `desc.color_attachment.clear_color`, load_op = CLEAR, store_op = STORE.  
  3. **Start the render pass**: `state_->render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target, 1, nullptr)`.  
  - In Vulkan: this begins a **VkRenderPass** that has one color attachment (the swapchain image), with clear and store. All subsequent draws render into this image until the pass ends.

- **`endRenderPass()`**:  
  - `SDL_EndGPURenderPass(state_->render_pass)` and clear the pointer.  
  - In Vulkan: ends the current render pass (no more draws to that attachment).

So **every draw** in the tutorial happens **inside** this single render pass, into the current swapchain image.

<a id="toc-55-buffers"></a>
### 5.5 Buffers (`createBuffer` / `writeBuffer` / `releaseBuffer`)

- **`createBuffer(BufferDesc)`**:  
  - Maps our `BufferUsage` to SDL_GPU flags (e.g. Vertex, Index).  
  - `SDL_CreateGPUBuffer(device, &info)` allocates a **VkBuffer** (and underlying memory) with the right usage.  
  - Returns a **Buffer** whose `handle` is the **SDL_GPUBuffer*** (wrapping that VkBuffer).

- **`writeBuffer(buffer, data, size)`**:  
  - **Staging**: create a **transfer (upload) buffer**: `SDL_CreateGPUTransferBuffer(..., SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size)`.  
  - **Map** it, **memcpy** CPU data into it, **unmap**.  
  - **Copy pass**: acquire a **separate** command buffer, begin a **copy pass** (`SDL_BeginGPUCopyPass`), call **`SDL_UploadToGPUBuffer`** to copy from the transfer buffer to the destination buffer, end the copy pass, **submit** that command buffer.  
  - Release the transfer buffer.  
  So the CPU data is uploaded via a **staging buffer + copy command**; the vertex/index buffers are GPU-local and used later in the render pass.

- **`releaseBuffer`**:  
  - `SDL_ReleaseGPUBuffer(device, buffer)` and set handle to null.  
  - Destroys the VkBuffer and frees the memory.

<a id="toc-56-shaders"></a>
### 5.6 Shaders (`createShader` / `releaseShader`)

- **`createShader(ShaderDesc)`**:  
  - Build **SDL_GPUShaderCreateInfo**: SPIR-V code and size, entry point, format = SPIR-V, stage = vertex or fragment, **num_uniform_buffers** from the desc (so the pipeline knows we have one uniform at set=1 binding=0).  
  - **SDL_CreateGPUShader(device, &info)** creates a **VkShaderModule** (and any descriptor set layout info for the uniform).  
  - Returns a **Shader** with handle = **SDL_GPUShader***.

- **`releaseShader`**:  
  - Release the SDL_GPUShader (destroy the module).

<a id="toc-57-graphics-pipeline"></a>
### 5.7 Graphics Pipeline (`createGraphicsPipeline` / `releaseGraphicsPipeline`)

**`createGraphicsPipeline(GraphicsPipelineDesc)`** is the biggest function. It:

1. **Vertex input**:  
   - From `VertexLayoutDesc` build **SDL_GPUVertexBufferDescription** (slot 0, stride = pitch, per-vertex) and **SDL_GPUVertexAttribute** array (location, buffer_slot, format, offset).  
   - So the pipeline knows: “vertex buffer slot 0 has this stride and these attributes at these locations.”

2. **Vertex input state**:  
   - **SDL_GPUVertexInputState** with the above buffer and attribute arrays.

3. **Rasterizer**:  
   - Fill mode, cull back faces, counter-clockwise front, no depth bias, depth clip on. Standard 3D setup.

4. **Multisample**:  
   - 1 sample (no MSAA).

5. **Depth/stencil**:  
   - Depth test and stencil disabled (we’re not using a depth buffer in this tutorial).

6. **Color target**:  
   - One color target, format B8G8R8A8_UNORM (matches swapchain), blend off (replace).

7. **Pipeline creation**:  
   - **SDL_GPUGraphicsPipelineCreateInfo** with vertex/fragment shaders, vertex input state, primitive type (triangle list/strip), rasterizer, multisample, depth/stencil, target info.  
   - **SDL_CreateGPUGraphicsPipeline(device, &pinfo)** creates a **VkPipeline** (and related Vulkan state).  
   - Returns **GraphicsPipeline** with handle = **SDL_GPUGraphicsPipeline***.

So the **pipeline** bakes in: shaders, vertex layout, topology, and fixed render state. At draw time we only **bind** this pipeline and the buffers; we don’t change these states per draw.

<a id="toc-58-recording-draws"></a>
### 5.8 Recording Draws (`setPipeline` / `pushVertexUniform` / `setVertexBuffer` / `setIndexBuffer` / `drawIndexed`)

These don’t submit anything; they **record** into the current command buffer (inside the current render pass) or **store** bindings for the next draw.

- **`setPipeline(pipeline)`**:  
  - Stores `state_->current_pipeline = pipeline.handle`. The actual **bind** happens in **`drawIndexed`**.

- **`pushVertexUniform(slot, data, size)`**:  
  - **SDL_PushGPUVertexUniformData(command_buffer, slot, data, size)**.  
  - This records “upload this data as the vertex-stage uniform for the given slot.” SDL_GPU will use it for the next draw (e.g. at set=1 binding=0 for our MVP).

- **`setVertexBuffer(slot, buffer, offset)`**:  
  - Stores `current_vertex_buffer` and `current_vertex_offset`. Only slot 0 is used in our backend.

- **`setIndexBuffer(buffer, offset)`**:  
  - Stores `current_index_buffer` and `current_index_offset`.

- **`drawIndexed(index_count)`**:  
  - **Bind pipeline**: if we have a current pipeline, **SDL_BindGPUGraphicsPipeline(render_pass, current_pipeline)**.  
  - **Bind vertex buffer(s)**: if we have a current vertex buffer, **SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1)**.  
  - **Bind index buffer**: if we have a current index buffer, **SDL_BindGPUIndexBuffer(render_pass, &binding, 32-bit)**.  
  - **Draw**: **SDL_DrawGPUIndexedPrimitives(render_pass, index_count, 1, 0, 0, 0)** → “draw index_count indices, 1 instance, first index 0, vertex offset 0, first instance 0.”  
  - In Vulkan terms that’s **vkCmdBindPipeline**, **vkCmdBindVertexBuffers**, **vkCmdBindIndexBuffer**, then **vkCmdDrawIndexed**.

So one **drawIndexed(36)** in the tutorial becomes: bind pipeline (with our shaders and layout), bind vertex buffer, bind index buffer, push uniform (already done), and issue one indexed draw of 36 indices (12 triangles).

---

<a id="toc-6-part-v-data-flow"></a>
## 6. Part V: End-to-End Data Flow

<a id="toc-61-one-frame"></a>
### 6.1 One Frame, Top to Bottom

1. **App**: `beginFrame()` → **Renderer** → **VulkanBackend::beginFrame()** → acquire **SDL_GPUCommandBuffer** (VkCommandBuffer).
2. **App**: `beginRenderPass(pass_desc)` → **VulkanBackend::beginRenderPass()** → wait/acquire **swapchain texture** → **SDL_BeginGPURenderPass** (start VkRenderPass, clear color attachment).
3. **App**: `setPipeline` / `pushVertexUniform` / `setVertexBuffer` / `setIndexBuffer` → backend **stores** pipeline and buffer handles and **records** uniform push.
4. **App**: `drawIndexed(36)` → **VulkanBackend::drawIndexed()** → **bind** pipeline, vertex buffer, index buffer on the **render pass**, then **record** vkCmdDrawIndexed(36, 1, 0, 0, 0).
5. **App**: `endRenderPass()` → **SDL_EndGPURenderPass** (end VkRenderPass).
6. **App**: `endFrame()` → **SDL_SubmitGPUCommandBuffer** → command buffer is submitted to the GPU queue; GPU executes it (clear, draw triangles with our shaders and buffers), then presents the swapchain image to the window.

So: **CPU records commands into a command buffer** (frame → render pass → bind + draw). **GPU executes** that buffer asynchronously and presents the result.

<a id="toc-62-where-data-lives"></a>
### 6.2 Where the Data Lives

- **Vertex/index data**:  
  - **CPU**: `cubeVertices`, `cubeIndices` in the example.  
  - **Upload**: `writeBuffer` copies them to a staging buffer, then a **copy command** puts them into the **GPU vertex/index buffers**.  
  - **Draw**: vertex shader reads from the **vertex buffer** (via the vertex layout) and from the **pushed uniform** (MVP). The **index buffer** tells the GPU which vertices form each triangle.

- **Uniform (MVP)**:  
  - **CPU**: built each frame (projection × view).  
  - **GPU**: **pushVertexUniform** records “make this data available to the vertex shader at the next draw.” So it’s **per-draw** (or per-frame if you only draw once per frame like in the tutorial).

- **Shaders**:  
  - **Disk**: GLSL (`cube.vert`, `cube.frag`) → compiled to SPIR-V (`.spv`).  
  - **CPU**: loaded with `loadFile`.  
  - **GPU**: **createShader** creates VkShaderModules; **createGraphicsPipeline** uses them. The pipeline is bound at draw time, so the GPU runs that vertex + fragment code for **drawIndexed(36)**.

<a id="toc-63-summary-table"></a>
### 6.3 Summary Table

| You call (example) | Renderer does | Vulkan backend does |
|--------------------|----------------|----------------------|
| `initialize(desc)` | SDL init, create window, create VulkanBackend, `vulkan->init(window, desc)` | Create SDL_GPU device, claim window (Vulkan device + swapchain) |
| `createBuffer(vbDesc)` | `vulkan->createBuffer` | SDL_CreateGPUBuffer → VkBuffer |
| `writeBuffer(vb, data, size)` | `vulkan->writeBuffer` | Staging buffer + copy pass + submit |
| `createShader(vsDesc)` | `vulkan->createShader` | SDL_CreateGPUShader → VkShaderModule |
| `createGraphicsPipeline(pipeDesc)` | `vulkan->createGraphicsPipeline` | Build vertex input, rasterizer, etc.; SDL_CreateGPUGraphicsPipeline → VkPipeline |
| `beginFrame()` | `vulkan->beginFrame` | SDL_AcquireGPUCommandBuffer |
| `beginRenderPass(pass_desc)` | `vulkan->beginRenderPass` | Acquire swapchain texture, SDL_BeginGPURenderPass (clear) |
| `setPipeline` / `pushVertexUniform` / `setVertexBuffer` / `setIndexBuffer` | Forward to vulkan | Store bindings; push uniform into command buffer |
| `drawIndexed(36)` | `vulkan->drawIndexed` | Bind pipeline, vertex buffer, index buffer; SDL_DrawGPUIndexedPrimitives(36, …) |
| `endRenderPass()` | `vulkan->endRenderPass` | SDL_EndGPURenderPass |
| `endFrame()` | `vulkan->endFrame` | Submit command buffer (GPU runs it and presents) |

That’s the full path from **tutorial/main.cpp** through the **public API** and the **Vulkan backend** to the actual GPU work.

---

<a id="toc-building-running"></a>
## Building and Running the Tutorial

From the **project root**:

1. **Compile shaders**: `zig build shaders` (produces `shaders/cube.vert.spv`, `shaders/cube.frag.spv`).
2. **Build**: `zig build` (builds the main app and the tutorial executable).
3. **Run tutorial**: `zig build run-tutorial` (runs the tutorial from the project root so it finds `shaders/`).

You should see a window titled “Tutorial – Cube” with a blue-ish clear color and a 3D cube. Closing the window or quitting the app ends the loop and cleans up.

For more high-level concepts and file layout, see **docs/GRAPHICS_GUIDE.md**.
