# Under the Hood of Our API

This guide is for when you're **not even familiar with our API yet**. It explains: what "our API" actually is, where it lives in the code, and **exactly what code runs** when you call something. No prior graphics knowledge assumed.

---

## Table of Contents

1. [What Is "Our API"?](#toc-what-is-api)
2. [Where Everything Lives in the Codebase](#toc-where-files)
3. [The Two Layers: You → Renderer → Backend](#toc-two-layers)
4. [What You See vs What You Don't](#toc-what-you-see)
5. [What Runs When You Call Something (Step by Step)](#toc-call-flow)
6. [What Are Handles (Buffer, Shader, GraphicsPipeline)?](#toc-handles)
7. [What Are Descriptors (BufferDesc, ShaderDesc, etc.)?](#toc-descriptors)
8. [What the Backend Keeps in Memory (State)](#toc-backend-state)
9. [One Complete Example: From createBuffer to the GPU](#toc-full-example)

---

<a id="toc-what-is-api"></a>
## 1. What Is "Our API"?

**"Our API"** means: the graphics interface that **your code** uses. You don't call Vulkan or SDL directly. You call one class and a few types, all in the **`gpu`** namespace.

In practice:

- You **include** one header: `#include <graphic.h>` (or the split headers under `graphic/`).
- You **create one object**: `gpu::Renderer renderer;`
- You **call methods on it**: `renderer.initialize(desc)`, `renderer.createBuffer(desc)`, `renderer.drawIndexed(36)`, etc.
- You **use a few types** to describe what you want (e.g. `gpu::BufferDesc`, `gpu::RenderPassDesc`) and to hold results (e.g. `gpu::Buffer`, `gpu::Shader`, `gpu::GraphicsPipeline`).

So "our API" = **the `Renderer` class + the types in the `gpu` namespace**. That's the only interface you need to know. Everything else (Vulkan, SDL, command buffers, swapchains) is **under the hood**.

---

<a id="toc-where-files"></a>
## 2. Where Everything Lives in the Codebase

All of this lives under the engine’s **graphic** code. Paths are from the **project root**.

**What you include (the “API” you see):**

| You include | File | What it gives you |
|-------------|------|-------------------|
| `#include <graphic.h>` | `engine/basic_library/include/graphic.h` | One include that pulls in the whole API |
| (or split) | `graphic/types.h` | Enums and small types: `Backend`, `BufferUsage`, `Extent2D`, `Color`, etc. |
| | `graphic/descriptors.h` | Structs you *fill in* to describe what you want: `RendererDesc`, `BufferDesc`, `ShaderDesc`, `RenderPassDesc`, `VertexLayoutDesc`, etc. |
| | `graphic/resources.h` | Handles and pipeline desc: `Buffer`, `Shader`, `GraphicsPipeline`, `GraphicsPipelineDesc` |
| | `graphic/renderer.h` | The **Renderer** class (the only class you use) |

**What you never include (the “under the hood”):**

| File | Who uses it | What it is |
|------|-------------|------------|
| `engine/basic_library/src/graphic/renderer.cpp` | Build system (compiled into the app) | Implements **Renderer**: window, events, and *forwards every GPU call* to the backend. |
| `engine/basic_library/include/graphic/detail/backend_vulkan.h` | Only `renderer.cpp` | Declares **VulkanBackend** (internal; you don’t include this). |
| `engine/basic_library/src/graphic/backend_vulkan.cpp` | Build system | Implements **VulkanBackend**: all the real work (SDL_GPU / Vulkan) for buffers, shaders, pipelines, frames, and draws. |

So:

- **Headers under `include/graphic/`** = our API (types, descriptors, `Renderer`).
- **`renderer.cpp`** = your calls land here first; it owns the window and the backend and forwards to it.
- **`backend_vulkan.cpp`** = the code that actually talks to the GPU (via SDL_GPU, which uses Vulkan).

---

<a id="toc-two-layers"></a>
## 3. The Two Layers: You → Renderer → Backend

When you call something like `renderer.createBuffer(desc)`, **two layers** of our code run:

```
  YOUR CODE                    OUR CODE (layer 1)              OUR CODE (layer 2)
  ---------                    -----------------              -----------------
  renderer.createBuffer(desc)  →  Renderer::createBuffer()   →  VulkanBackend::createBuffer()
       (your .cpp)                   (renderer.cpp)                (backend_vulkan.cpp)
```

- **Layer 1 – Renderer** (`renderer.cpp`):  
  - Owns the window (SDL) and the backend.  
  - Every method checks “do we have a Vulkan backend?” and, if yes, calls the same method on the backend.  
  - It does **no** GPU work itself; it just forwards.

- **Layer 2 – Backend** (`backend_vulkan.cpp`):  
  - Holds the real GPU state (device, command buffer, render pass, current pipeline/buffers).  
  - Implements each operation using **SDL_GPU** (which in turn uses **Vulkan**).  
  - So “under the hood” of our API really means: “what **Renderer** does (forward) and what **VulkanBackend** does (SDL_GPU/Vulkan).”

If we added a WebGPU or OpenGL backend later, we’d have a second (and third) “layer 2” implementation; your code and `renderer.cpp` would stay the same.

---

<a id="toc-what-you-see"></a>
## 4. What You See vs What You Don't

**You see and use:**

- **Renderer** – one object; you call `initialize`, `createBuffer`, `beginFrame`, `drawIndexed`, etc.
- **Descriptors** – structs you fill in and pass *into* the API: `RendererDesc`, `BufferDesc`, `ShaderDesc`, `RenderPassDesc`, `VertexLayoutDesc`, etc. They describe *what* you want (size, usage, clear color, …).
- **Handles** – structs the API *returns* or that you pass back in: `Buffer`, `Shader`, `GraphicsPipeline`. They don’t contain the GPU object; they hold an opaque **handle** (a pointer) the backend assigns.
- **Types** – enums and small data: `Backend`, `BufferUsage`, `Extent2D`, `Color`, `VertexFormat`, etc.

**You never see:**

- **Renderer::Impl** – the private implementation of `Renderer`: the SDL window, the backend pointer, and flags. Stored in `renderer.cpp`.
- **VulkanBackend** – the class that does the real work. You never mention it; only `Renderer` uses it.
- **VulkanBackend::State** – the backend’s internal state (device, command buffer, render pass, current pipeline and buffers). Stored in `backend_vulkan.cpp`.
- **SDL** or **SDL_GPU** or **Vulkan** – you never call them directly; the backend does.

So “under the hood” = **Impl** (renderer) + **VulkanBackend** and its **State**.

---

<a id="toc-call-flow"></a>
## 5. What Runs When You Call Something (Step by Step)

For every call you make, the pattern is the same: **your code** → **Renderer method** → **backend method** (when backend is Vulkan). Here’s that for each group of operations.

---

### 5.1 Startup and shutdown

**You write:**  
`renderer.initialize(renderer_desc);`

**What runs:**

1. **`renderer.cpp`: `Renderer::initialize(desc)`**
   - Creates/uses internal state **`impl_`** (type **`Renderer::Impl`**).
   - Sets `impl_->backend` from `desc.device.backend`.
   - Calls **SDL_Init(SDL_INIT_VIDEO)**.
   - Creates the window with **SDL_CreateWindow(...)** and stores it in **`impl_->window`**.
   - If backend is Vulkan: creates **`impl_->vulkan = std::make_unique<VulkanBackend>()`**, then calls **`impl_->vulkan->init(impl_->window, desc)`**.
   - So: **Renderer** does SDL + window; **VulkanBackend::init** does the GPU device and claiming the window for that device.

2. **`backend_vulkan.cpp`: `VulkanBackend::init(window, desc)`**
   - Creates **`state_`** (type **`VulkanBackend::State`**).
   - Stores the window in **`state_->window`**.
   - Creates the GPU device: **`state_->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, ..., "vulkan")`**.
   - Claims the window for that device: **`SDL_ClaimWindowForGPUDevice(state_->device, state_->window)`** (so the device can render into the window).

So “initialize” = SDL + window (Renderer) + Vulkan device + window claim (VulkanBackend).

---

**You write:**  
`renderer.shutdown();`

**What runs:**

1. **`renderer.cpp`: `Renderer::shutdown()`**
   - If **`impl_->vulkan`** exists: calls **`impl_->vulkan->shutdown()`**, then **`impl_->vulkan.reset()`**.
   - Destroys the window: **`SDL_DestroyWindow(impl_->window)`**.
   - If SDL was initialized: **`SDL_Quit()`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::shutdown()`**
   - If a render pass is active: **`SDL_EndGPURenderPass(...)`**.
   - Releases the window from the device: **`SDL_ReleaseWindowFromGPUDevice(...)`**.
   - Destroys the device: **`SDL_DestroyGPUDevice(state_->device)`**.
   - Clears **`state_`**.

So shutdown = backend tears down GPU and window claim, then Renderer tears down window and SDL.

---

### 5.2 Frame and render pass

**You write:**  
`renderer.beginFrame();`

**What runs:**

1. **`renderer.cpp`:**  
   If we have a Vulkan backend, it calls **`impl_->vulkan->beginFrame()`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::beginFrame()`**  
   Acquires a **command buffer** for this frame: **`state_->command_buffer = SDL_AcquireGPUCommandBuffer(state_->device)`**.  
   So “begin frame” under the hood = “get the command list we’ll record into for this frame.”

---

**You write:**  
`renderer.beginRenderPass(pass_desc);`

**What runs:**

1. **`renderer.cpp`** forwards to **`impl_->vulkan->beginRenderPass(desc)`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::beginRenderPass(desc)`**
   - Waits for and acquires the **swapchain image** (the texture that is the current “screen”): **`SDL_WaitAndAcquireGPUSwapchainTexture(state_->command_buffer, state_->window, &swapchain_texture, ...)`**.
   - Fills a **color target** with that texture and your **clear color** from **`desc.color_attachment.clear_color`**.
   - Starts the render pass: **`state_->render_pass = SDL_BeginGPURenderPass(state_->command_buffer, &color_target, 1, nullptr)`**.

So “begin render pass” = acquire the image we’ll draw into, then start a render pass that clears and renders into it.

---

**You write:**  
`renderer.endRenderPass();` then `renderer.endFrame();`

**What runs:**

1. **endRenderPass** (in `renderer.cpp` → **`VulkanBackend::endRenderPass()`**):  
   **`SDL_EndGPURenderPass(state_->render_pass)`**, then **`state_->render_pass = nullptr`**. So we stop recording draws to the screen.

2. **endFrame** (in `renderer.cpp` → **`VulkanBackend::endFrame()`**):  
   Ensures the render pass is ended, then **`SDL_SubmitGPUCommandBuffer(state_->command_buffer)`** (the GPU runs the recorded commands), then **`state_->command_buffer = nullptr`**. The driver then presents the swapchain image to the window. So “end frame” = submit the command buffer and present.

---

### 5.3 Buffers

**You write:**  
`gpu::Buffer buf = renderer.createBuffer(desc);`

**What runs:**

1. **`renderer.cpp`: `Renderer::createBuffer(desc)`**  
   Returns **`impl_->vulkan->createBuffer(desc)`** (or an empty Buffer if no Vulkan backend).

2. **`backend_vulkan.cpp`: `VulkanBackend::createBuffer(desc)`**
   - Builds SDL_GPU buffer usage from **`desc.usage`** (e.g. Vertex, Index).
   - Creates the buffer: **`SDL_CreateGPUBuffer(state_->device, &info)`**.
   - Returns **`Buffer{ buf }`** – a **Buffer** whose **`handle`** is that **SDL_GPUBuffer***.

So “createBuffer” = ask the backend to create a GPU buffer and give you back a handle to it.

---

**You write:**  
`renderer.writeBuffer(buf, data, size);`

**What runs:**

1. **`renderer.cpp`** forwards to **`impl_->vulkan->writeBuffer(buffer, data, size)`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::writeBuffer(...)`**
   - Creates a **temporary transfer (staging) buffer** and **maps** it.
   - **memcpy**’s your **data** into that staging buffer, then **unmap**.
   - **Acquires a command buffer**, starts a **copy pass**, records **SDL_UploadToGPUBuffer** (copy from staging → your buffer), **ends the copy pass**, **submits** that command buffer.
   - Releases the staging buffer.

So “writeBuffer” = copy your CPU data into a staging buffer, then record and submit a GPU copy from staging into the buffer you created. That’s how data gets “onto the GPU” under the hood.

---

**You write:**  
`renderer.releaseBuffer(buf);`

**What runs:**

1. **`renderer.cpp`** calls **`impl_->vulkan->releaseBuffer(buffer)`**, then sets **`buffer.handle = nullptr`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::releaseBuffer(buffer)`**  
   **`SDL_ReleaseGPUBuffer(state_->device, buffer.handle)`** – destroys the GPU buffer. So “releaseBuffer” = destroy the underlying GPU buffer and clear your handle.

---

### 5.4 Shaders and pipeline

**You write:**  
`gpu::Shader vs = renderer.createShader(vsDesc);`

**What runs:**

1. **`renderer.cpp`** forwards to **`impl_->vulkan->createShader(desc)`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::createShader(desc)`**
   - Fills **SDL_GPUShaderCreateInfo** from **desc** (code = SPIR-V, size, entry point, stage, **num_uniform_buffers**).
   - **`SDL_CreateGPUShader(state_->device, &info)`** creates the shader module.
   - Returns **`Shader{ sh }`** – **Shader.handle** is that **SDL_GPUShader***.

So “createShader” = create a GPU shader module from the SPIR-V you passed in.

---

**You write:**  
`gpu::GraphicsPipeline pipe = renderer.createGraphicsPipeline(pipeDesc);`

**What runs:**

1. **`renderer.cpp`** forwards to **`impl_->vulkan->createGraphicsPipeline(desc)`**.

2. **`backend_vulkan.cpp`: `VulkanBackend::createGraphicsPipeline(desc)`**
   - Builds **vertex input** from **desc.vertex_layout** (stride, attributes).
   - Sets fixed state (rasterizer, multisample, depth/stencil, color target).
   - Fills **SDL_GPUGraphicsPipelineCreateInfo** with the two shaders from **desc**, vertex input, primitive type, and that state.
   - **`SDL_CreateGPUGraphicsPipeline(state_->device, &pinfo)`** creates the pipeline.
   - Returns **`GraphicsPipeline{ pipe }`** – **handle** is that **SDL_GPUGraphicsPipeline***.

So “createGraphicsPipeline” = take the two shaders and the vertex layout (and other state) and create one “pipeline” object the GPU will use when drawing.

---

### 5.5 Drawing

**You write:**  
`renderer.setPipeline(pipeline);`  
`renderer.pushVertexUniform(0, &mvp, sizeof(mvp));`  
`renderer.setVertexBuffer(0, vertexBuffer, 0);`  
`renderer.setIndexBuffer(indexBuffer, 0);`  
`renderer.drawIndexed(36);`

**What runs:**

1. **setPipeline**  
   **renderer.cpp** → **VulkanBackend::setPipeline(pipeline)**.  
   Backend does: **`state_->current_pipeline = pipeline.handle`**. So we *remember* which pipeline to use at the next draw.

2. **pushVertexUniform**  
   **renderer.cpp** → **VulkanBackend::pushVertexUniform(slot, data, size)**.  
   Backend does: **`SDL_PushGPUVertexUniformData(state_->command_buffer, slot, data, size)`**. So we *record* “make this data available to the vertex shader” on the current command buffer.

3. **setVertexBuffer / setIndexBuffer**  
   **renderer.cpp** → backend.  
   Backend stores **`state_->current_vertex_buffer`** and **`state_->current_vertex_offset`** (and same for index). So we *remember* which buffers to bind at the next draw.

4. **drawIndexed(36)**  
   **renderer.cpp** → **VulkanBackend::drawIndexed(36)**.  
   Backend:
   - If we have a current pipeline: **SDL_BindGPUGraphicsPipeline(state_->render_pass, state_->current_pipeline)**.
   - If we have a current vertex buffer: **SDL_BindGPUVertexBuffers(state_->render_pass, 0, &binding, 1)**.
   - If we have a current index buffer: **SDL_BindGPUIndexBuffer(state_->render_pass, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT)**.
   - **SDL_DrawGPUIndexedPrimitives(state_->render_pass, 36, 1, 0, 0, 0)**.

So “drawIndexed” under the hood = bind the pipeline and buffers we remembered, then record “draw 36 indices” into the current render pass. The actual execution happens when you **endFrame** and the command buffer is submitted.

---

<a id="toc-handles"></a>
## 6. What Are Handles (Buffer, Shader, GraphicsPipeline)?

You don’t get the “real” GPU object. You get a **handle**: a small struct that holds a **pointer** the backend assigns.

From **`graphic/resources.h`**:

```cpp
struct Buffer {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};
struct Shader {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};
struct GraphicsPipeline {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};
```

- **You:** create a buffer with **createBuffer**; you get back a **Buffer** whose **handle** is non-null. You pass that **Buffer** (by value or reference) back into **writeBuffer**, **setVertexBuffer**, **releaseBuffer**, etc. You never look at **handle** yourself; you just pass the **Buffer** around.
- **Backend:** when it creates the real GPU buffer, it stores the **SDL_GPUBuffer*** (or in the future, a WebGPU/OpenGL object) and returns **Buffer{ that pointer }**. When you pass the **Buffer** back in, the backend **casts `buffer.handle`** back to **SDL_GPUBuffer*** and uses it. So **handle** is **opaque**: you don’t know or care what’s inside; the backend knows it’s its own pointer.

So “under the hood,” **Buffer** (and **Shader**, **GraphicsPipeline**) are just a **void*** that the backend fills in and later uses. The “real” object lives in the backend (and the GPU); the handle is your ticket to refer to it.

---

<a id="toc-descriptors"></a>
## 7. What Are Descriptors (BufferDesc, ShaderDesc, etc.)?

**Descriptors** are **structs you fill in** to describe *what* you want. They don’t create anything by themselves. You pass them *into* the API; the backend reads them and creates the real thing.

Examples from **`graphic/descriptors.h`** (and **types.h**):

- **RendererDesc** – backend (Vulkan/…), window title/size, etc. You pass it to **initialize**.
- **BufferDesc** – **size** and **usage** (Vertex, Index, …). You pass it to **createBuffer**.
- **ShaderDesc** – **stage** (Vertex/Fragment), **code** (SPIR-V), **code_size**, **entry_point**, **num_uniform_buffers**. You pass it to **createShader**.
- **VertexLayoutDesc** – **stride**, **attribute_count**, **attributes** (location, format, offset). You pass it inside **GraphicsPipelineDesc** to **createGraphicsPipeline**.
- **RenderPassDesc** – **color_attachment** (clear color, format). You pass it to **beginRenderPass**.

So descriptors = **options/configuration**. The backend reads them and does the actual create/begin calls. They’re not handles; they’re “how you want it.”

---

<a id="toc-backend-state"></a>
## 8. What the Backend Keeps in Memory (State)

The Vulkan backend keeps one **State** struct (in **`backend_vulkan.cpp`**):

```cpp
struct VulkanBackend::State {
    SDL_Window* window = nullptr;
    SDL_GPUDevice* device = nullptr;
    SDL_GPUCommandBuffer* command_buffer = nullptr;
    SDL_GPURenderPass* render_pass = nullptr;
    SDL_GPUGraphicsPipeline* current_pipeline = nullptr;
    SDL_GPUBuffer* current_vertex_buffer = nullptr;
    Uint32 current_vertex_offset = 0;
    SDL_GPUBuffer* current_index_buffer = nullptr;
    Uint32 current_index_offset = 0;
};
```

- **window** – the window we render into (passed in at **init**).
- **device** – the GPU device (created in **init**). Used for creating buffers, shaders, pipelines, and command buffers.
- **command_buffer** – the command list for the *current frame*. Set in **beginFrame**, submitted in **endFrame**, then cleared.
- **render_pass** – the active render pass (we’re drawing into the swapchain image). Set in **beginRenderPass**, ended in **endRenderPass**, then cleared.
- **current_pipeline**, **current_vertex_buffer**, **current_vertex_offset**, **current_index_buffer**, **current_index_offset** – *remembered* so that when you call **drawIndexed**, we know which pipeline and buffers to bind before recording the draw.

So “under the hood” the backend is just this state plus the SDL_GPU/Vulkan calls that use it. You never see **State**; you only see the effect of it (e.g. draw uses the pipeline and buffers you last set).

---

<a id="toc-full-example"></a>
## 9. One Complete Example: From createBuffer to the GPU

Here’s one path from “you want a vertex buffer” to “data is on the GPU,” with every layer.

**1. You (e.g. in `tutorial/main.cpp`):**

```cpp
gpu::BufferDesc vbDesc{};
vbDesc.size = sizeof(vertices);
vbDesc.usage = gpu::BufferUsage::Vertex;
gpu::Buffer vertexBuffer = renderer.createBuffer(vbDesc);
renderer.writeBuffer(vertexBuffer, vertices, sizeof(vertices));
```

**2. createBuffer:**

- **Renderer::createBuffer(vbDesc)** (renderer.cpp) → **impl_->vulkan->createBuffer(vbDesc)**.
- **VulkanBackend::createBuffer(vbDesc)** (backend_vulkan.cpp):  
  - Converts **vbDesc.usage** to SDL_GPU flags.  
  - **SDL_CreateGPUBuffer(device, &info)** → real GPU buffer.  
  - Returns **Buffer{ that SDL_GPUBuffer* }**.  
  So **vertexBuffer.handle** now points to the backend’s buffer object.

**3. writeBuffer:**

- **Renderer::writeBuffer(vertexBuffer, vertices, sizeof(vertices))** → **impl_->vulkan->writeBuffer(vertexBuffer, vertices, size)**.
- **VulkanBackend::writeBuffer(...)** (backend_vulkan.cpp):  
  - Creates a **staging** (upload) buffer.  
  - Maps it, **memcpy(vertices)** into it, unmaps.  
  - Acquires a **command buffer**, starts a **copy pass**, **SDL_UploadToGPUBuffer** (staging → vertexBuffer.handle), ends copy pass, **submits** that command buffer.  
  - Releases the staging buffer.  
  So the GPU has run a copy command that put your **vertices** into the buffer **vertexBuffer** refers to.

**4. Later, when you draw:**

- You call **setVertexBuffer(0, vertexBuffer, 0)**.  
  Backend sets **state_->current_vertex_buffer = vertexBuffer.handle** (and offset).
- You call **drawIndexed(36)**.  
  Backend **binds** that buffer to the render pass (**SDL_BindGPUVertexBuffers**) and records **SDL_DrawGPUIndexedPrimitives(36, ...)**.  
  So the “draw” command says: “use the pipeline and buffers we just bound; draw 36 indices.”

**5. When you endFrame:**

- Backend **submits** the command buffer. The GPU then executes: clear, bind pipeline, bind vertex (and index) buffer, use pushed uniform, draw 36 indices. So the triangle list is actually drawn using the data you put in the buffer.

That’s the full “under the hood” path: **your call** → **Renderer** (forward) → **VulkanBackend** (SDL_GPU/Vulkan) → **GPU**.

---

## Summary

- **Our API** = **Renderer** + **gpu** types (descriptors + handles). You only include **graphic.h** (or the split headers) and call **renderer.***.
- **Under the hood** = **Renderer::Impl** (window + backend) in **renderer.cpp**, and **VulkanBackend** + **VulkanBackend::State** in **backend_vulkan.cpp**.
- Every call you make goes **Renderer method** → **same method on VulkanBackend**; the backend then uses **State** and **SDL_GPU** (Vulkan) to do the work.
- **Handles** (Buffer, Shader, GraphicsPipeline) are opaque: they hold a **void*** the backend assigns and later uses. **Descriptors** are structs you fill in to describe what you want; the backend reads them when you call create/begin.

If you open **renderer.cpp** and **backend_vulkan.cpp** and search for the function name you care about, you can follow this exact path for any call.
