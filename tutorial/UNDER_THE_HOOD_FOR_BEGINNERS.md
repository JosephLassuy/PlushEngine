# Under the Hood for Complete Beginners

This guide is for anyone **very new to graphics programming**. It explains what’s going on in plain language, then shows **both** the code you write (our API) and the code that actually runs under the hood, with real examples from this project.

---

## Table of Contents

1. [Graphics in 60 Seconds](#toc-graphics-60-sec)
2. [Our API in 60 Seconds](#toc-api-60-sec)
3. [How Your Call Reaches the GPU (The Two Layers)](#toc-two-layers)
4. [Example 1: Starting Up (Window + GPU)](#toc-example-startup)
5. [Example 2: Creating a Buffer and Uploading Data](#toc-example-buffer)
6. [Example 3: Creating Shaders](#toc-example-shaders)
7. [Example 4: The Frame Loop (One Image per Loop)](#toc-example-frame)
8. [Example 5: Drawing (Pipeline, Buffers, One Draw)](#toc-example-draw)
9. [Putting It All Together (One Frame, Start to Finish)](#toc-putting-together)

---

<a id="toc-graphics-60-sec"></a>
## 1. Graphics in 60 Seconds

**What we’re doing:** We want to show a picture on the screen (a window) and change it over time (e.g. 60 times per second). That picture is made of **pixels**. We don’t set each pixel by hand; we tell the **GPU** (the graphics card) to draw **shapes** (usually **triangles**) for us.

**CPU vs GPU:** Your normal code runs on the **CPU**. The **GPU** is a separate chip that’s very good at doing the same small task millions of times (e.g. “for each pixel inside this triangle, what color?”). So we **prepare** data on the CPU (positions of corners, colors, a small “program” for the GPU), then **send** that to the GPU. The GPU then **draws** the triangles and fills the pixels. That’s “graphics programming” in a nutshell.

**Window:** The rectangle you see on screen is a **window**. We need to create it (with a library like SDL) and tell the GPU “draw into this window.”

**Frame:** One image we show is called a **frame**. Each time we want to show a new image we: clear the window, draw our triangles, then “present” the result so the user sees it. So one loop iteration = one frame.

**Vertices and triangles:** A **vertex** is one “corner” of your shape (position, color, etc.). A **triangle** is three vertices. The GPU draws triangles; we give it a list of vertices and (optionally) a list of “which three vertices form each triangle” (indices). So “draw a cube” = draw 12 triangles (two per face), each with 3 vertices.

**Shaders:** A **shader** is a tiny program that runs on the GPU. We use two: the **vertex shader** (runs per vertex: e.g. “multiply position by the camera matrix”) and the **fragment shader** (runs per pixel: e.g. “output this color”). We write them in a language called GLSL, compile them to a binary (SPIR-V), and the GPU runs that binary.

**Buffers:** Data that the GPU uses (vertex positions, colors, indices) must live in **GPU memory**. A **buffer** is just a chunk of that memory. We **create** a buffer, **upload** data from the CPU into it, then tell the GPU “use this buffer when you draw.”

**Pipeline:** The **pipeline** is a bundle of “how to draw”: which vertex and fragment shaders to use, how to read the vertex buffer (stride, attributes), and fixed options (e.g. triangle list, culling). We create it once, then “bind” it when we draw.

So the flow is: **create window and GPU device → create buffers and upload data → create shaders and pipeline → each frame: clear, bind pipeline and buffers, draw triangles, present.**

---

<a id="toc-api-60-sec"></a>
## 2. Our API in 60 Seconds

**Our API** = the one interface you use. You don’t call the GPU or the OS directly. You use:

- **One class:** `gpu::Renderer`. You create it, call `initialize(...)`, then call methods like `createBuffer`, `beginFrame`, `drawIndexed`, etc.
- **Descriptor structs:** You fill these in to *describe* what you want (e.g. buffer size and usage, clear color). Examples: `gpu::RendererDesc`, `gpu::BufferDesc`, `gpu::RenderPassDesc`.
- **Handle structs:** The API *returns* these (or you pass them back in). They don’t hold the real GPU object; they hold an opaque **handle** (a pointer) the engine assigns. Examples: `gpu::Buffer`, `gpu::Shader`, `gpu::GraphicsPipeline`.

You include `#include <graphic.h>` and use only the `gpu::` namespace. Everything else (Vulkan, SDL, command buffers) is **under the hood**.

---

<a id="toc-two-layers"></a>
## 3. How Your Call Reaches the GPU (The Two Layers)

When you call something like `renderer.createBuffer(desc)`:

1. **Your code** (e.g. `tutorial/main.cpp`) calls `renderer.createBuffer(desc)`.
2. **Layer 1 – Renderer** (`renderer.cpp`): The method `Renderer::createBuffer(desc)` runs. It doesn’t do the GPU work; it just calls the **backend**: `impl_->vulkan->createBuffer(desc)`.
3. **Layer 2 – Backend** (`backend_vulkan.cpp`): The method `VulkanBackend::createBuffer(desc)` runs. It talks to the GPU using **SDL_GPU** (which uses **Vulkan**): it creates a real GPU buffer and returns a `Buffer` whose `handle` points to that object.

So: **you → Renderer (forward) → VulkanBackend (real work)**. Every example below shows both your API code and the actual “under the hood” code in these two files.

---

<a id="toc-example-startup"></a>
## 4. Example 1: Starting Up (Window + GPU)

**What we want:** A window on screen and a connection to the GPU so we can create buffers, shaders, and draw. We do this once at startup.

---

### What you write (our API)

```cpp
#include <graphic.h>

gpu::Renderer renderer;
gpu::RendererDesc desc{};
desc.device.backend = gpu::Backend::Vulkan;
desc.surface.window.title = "My App";
desc.surface.window.size = {1280, 720};

if (!renderer.initialize(desc)) {
    return 1;  // error
}
```

You create a **Renderer**, fill a **RendererDesc** (backend = Vulkan, window title and size), and call **initialize**. You never see the window or GPU object; the renderer owns them.

---

### Under the hood – Layer 1: Renderer (renderer.cpp)

When you call `renderer.initialize(desc)`, this is the code that runs first (simplified; real file: `engine/basic_library/src/graphic/renderer.cpp`):

```cpp
bool Renderer::initialize(const RendererDesc& desc) {
    // 1) Remember which backend we're using (Vulkan, etc.)
    impl_->backend = desc.device.backend;

    // 2) Start the video system (SDL). Needed to create a window.
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        // error ...
    }
    impl_->sdl_initialized = true;

    // 3) Create the actual window: title, width, height, flags
    impl_->window = SDL_CreateWindow(
        desc.surface.window.title,
        static_cast<int>(desc.surface.window.size.width),
        static_cast<int>(desc.surface.window.size.height),
        window_flags
    );
    if (!impl_->window) {
        shutdown();
        return false;
    }

    // 4) Create the GPU backend and connect it to the window
    switch (desc.device.backend) {
        case Backend::Vulkan:
            impl_->vulkan = std::make_unique<VulkanBackend>();
            if (!impl_->vulkan->init(impl_->window, desc)) {
                impl_->vulkan.reset();
                shutdown();
                return false;
            }
            return true;
        // ... other backends not implemented yet
    }
    return false;
}
```

**In plain English:** The renderer (1) turns on SDL, (2) creates the window with your title and size, (3) creates a **VulkanBackend** and calls **init** with that window. So the window is created here; the GPU device is created inside **VulkanBackend::init**.

---

### Under the hood – Layer 2: Backend (backend_vulkan.cpp)

When the renderer calls `impl_->vulkan->init(impl_->window, desc)`, this runs (real file: `engine/basic_library/src/graphic/backend_vulkan.cpp`):

```cpp
bool VulkanBackend::init(void* window, const RendererDesc& desc) {
    // 1) Allocate our internal state (device, command buffer, etc.)
    state_ = std::make_unique<State>();
    state_->window = static_cast<SDL_Window*>(window);

    // 2) Create the GPU "device" – this is the connection to the graphics card.
    //    We say: use SPIR-V shaders, optional validation, and the "vulkan" driver.
    state_->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV,
        desc.device.enable_validation,
        "vulkan"
    );
    if (!state_->device) {
        // error ...
    }

    // 3) Tell the device "you will draw into this window." This sets up the
    //    swapchain (the chain of images that get shown in the window).
    if (!SDL_ClaimWindowForGPUDevice(state_->device, state_->window)) {
        // error ...
    }
    return true;
}
```

**In plain English:** The backend (1) stores the window and allocates its **State**, (2) creates the **GPU device** (the object we use to create buffers, shaders, and command buffers), (3) **claims the window** for that device so we can render into it. After this, we’re ready to create buffers and shaders and to draw.

---

<a id="toc-example-buffer"></a>
## 5. Example 2: Creating a Buffer and Uploading Data

**What we want:** A chunk of GPU memory to hold vertex data (e.g. positions and colors of the corners of a cube). We create the buffer, then copy our CPU data into it.

---

### What you write (our API)

```cpp
// Our vertex data on the CPU (e.g. 8 corners of a cube, each with position + color)
struct Vertex { float x, y, z;  float r, g, b; };
Vertex vertices[] = { /* ... */ };

// Describe the buffer: how many bytes, and that it's for vertices
gpu::BufferDesc vbDesc{};
vbDesc.size = sizeof(vertices);
vbDesc.usage = gpu::BufferUsage::Vertex;

// Create the buffer (GPU allocates memory)
gpu::Buffer vertexBuffer = renderer.createBuffer(vbDesc);

// Copy our CPU data into the GPU buffer
renderer.writeBuffer(vertexBuffer, vertices, sizeof(vertices));
```

You never touch “GPU memory” directly; you say how big the buffer is and how it’s used, then you **write** your data into it.

---

### Under the hood – createBuffer

**Renderer (renderer.cpp):** It just forwards to the backend.

```cpp
Buffer Renderer::createBuffer(const BufferDesc& desc) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->createBuffer(desc);
    return Buffer{};
}
```

**Backend (backend_vulkan.cpp):** It builds the low-level description and creates the real buffer.

```cpp
Buffer VulkanBackend::createBuffer(const BufferDesc& desc) {
    if (!state_ || !state_->device || desc.size == 0) return Buffer{};

    // Translate our "usage" (Vertex, Index, ...) into SDL_GPU flags
    SDL_GPUBufferCreateInfo info{};
    info.usage = toSdlBufferUsage(desc.usage);   // e.g. Vertex → SDL_GPU_BUFFERUSAGE_VERTEX
    info.size = static_cast<Uint32>(desc.size);
    info.props = 0;

    // Ask the GPU to allocate a buffer of that size and usage
    SDL_GPUBuffer* buf = SDL_CreateGPUBuffer(state_->device, &info);

    // Return our handle: we store the pointer so we can use it later
    return Buffer{buf};
}
```

**In plain English:** The backend converts our **BufferDesc** into what SDL_GPU (Vulkan) expects, then calls **SDL_CreateGPUBuffer**. The returned **Buffer** holds that pointer as **handle**; you pass **vertexBuffer** around and the backend uses **vertexBuffer.handle** when you bind it for drawing.

---

### Under the hood – writeBuffer

The CPU can’t write directly into most GPU buffers, so we use a **staging buffer**: we copy your data into a temporary buffer the CPU can write to, then we **record a copy command** so the GPU copies from that temporary buffer into your vertex buffer. Then we **submit** that command.

**Backend (backend_vulkan.cpp):**

```cpp
bool VulkanBackend::writeBuffer(Buffer& buffer, const void* data, u64 size) {
    // ...
    SDL_GPUBuffer* buf = static_cast<SDL_GPUBuffer*>(buffer.handle);

    // 1) Create a temporary "transfer" (staging) buffer that the CPU can write to
    SDL_GPUTransferBufferCreateInfo tinfo{};
    tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tinfo.size = static_cast<Uint32>(size);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(state_->device, &tinfo);

    // 2) Map it (get a CPU pointer), copy your data in, unmap
    void* mapped = SDL_MapGPUTransferBuffer(state_->device, transfer, false);
    std::memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(state_->device, transfer);

    // 3) Record a copy: "copy from transfer buffer into the real vertex buffer"
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state_->device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);   // src = transfer, dst = buf
    SDL_EndGPUCopyPass(copy);

    // 4) Submit so the GPU actually runs the copy
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(state_->device, transfer);
    return true;
}
```

**In plain English:** We create a small upload buffer, copy your **data** into it, then record “GPU: copy from this upload buffer into the vertex buffer” and submit that. After this, your vertex data is in GPU memory and ready to use when we draw.

---

<a id="toc-example-shaders"></a>
## 6. Example 3: Creating Shaders

**What we want:** The GPU needs two small programs (vertex and fragment shader). We write them in GLSL, compile to SPIR-V offline, load the bytes, and give them to the API. The backend creates a **shader module** the GPU can use.

---

### What you write (our API)

```cpp
// Load compiled shader bytes (you ran: zig build shaders)
std::vector<std::uint8_t> vertSpv = loadFile("shaders/cube.vert.spv");
std::vector<std::uint8_t> fragSpv = loadFile("shaders/cube.frag.spv");

gpu::ShaderDesc vsDesc{};
vsDesc.stage = gpu::ShaderStage::Vertex;
vsDesc.code = vertSpv.data();
vsDesc.code_size = vertSpv.size();
vsDesc.entry_point = "main";
vsDesc.num_uniform_buffers = 1;   // we have one uniform (e.g. MVP matrix)

gpu::Shader vertShader = renderer.createShader(vsDesc);

gpu::ShaderDesc fsDesc{};
fsDesc.stage = gpu::ShaderStage::Fragment;
fsDesc.code = fragSpv.data();
fsDesc.code_size = fragSpv.size();
fsDesc.entry_point = "main";
gpu::Shader fragShader = renderer.createShader(fsDesc);
```

You describe each shader (stage, SPIR-V bytes, entry point, and how many uniform buffers the vertex shader has). The API returns a **Shader** handle.

---

### Under the hood – createShader (backend_vulkan.cpp)

```cpp
Shader VulkanBackend::createShader(const ShaderDesc& desc) {
    // ...
    SDL_GPUShaderCreateInfo info{};
    info.code_size = desc.code_size;
    info.code = static_cast<const Uint8*>(desc.code);      // SPIR-V bytes
    info.entrypoint = desc.entry_point ? desc.entry_point : "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;             // we use SPIR-V
    info.stage = (desc.stage == ShaderStage::Vertex)
        ? SDL_GPU_SHADERSTAGE_VERTEX
        : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_uniform_buffers = desc.num_uniform_buffers;

    SDL_GPUShader* sh = SDL_CreateGPUShader(state_->device, &info);
    return Shader{sh};
}
```

**In plain English:** The backend fills an SDL_GPU shader descriptor from our **ShaderDesc** (SPIR-V code, stage, entry point, uniform count) and calls **SDL_CreateGPUShader**. That creates a shader module on the GPU. We return a **Shader** whose **handle** is that pointer. Later, when we create the **pipeline**, we attach these two shaders to it.

---

<a id="toc-example-frame"></a>
## 7. Example 4: The Frame Loop (One Image per Loop)

**What we want:** Each time through the game loop we produce **one frame**: we get a “command list” for this frame, say “we’re drawing into the window and we clear it to this color,” do our draws, then submit the list so the GPU runs it and the image appears.

---

### What you write (our API)

```cpp
gpu::RenderPassDesc pass_desc{};
pass_desc.color_attachment.clear_color = {0.12f, 0.18f, 0.32f, 1.0f};  // dark blue

while (renderer.pollEvents()) {

    if (!renderer.beginFrame()) break;

    if (!renderer.beginRenderPass(pass_desc)) break;

    // ... set pipeline, buffers, uniforms, drawIndexed(...) ...

    renderer.endRenderPass();

    if (!renderer.endFrame()) break;
}
```

- **beginFrame** = “start recording commands for this frame.”
- **beginRenderPass** = “we’re drawing into the window; clear it to this color.”
- **endRenderPass** = “done recording draws to the window.”
- **endFrame** = “submit the recorded commands so the GPU runs them and the image is shown.”

---

### Under the hood – beginFrame (backend_vulkan.cpp)

```cpp
bool VulkanBackend::beginFrame() {
    if (!state_ || !state_->device) return false;
    if (state_->command_buffer) return true;   // already have one

    // Get a command buffer from the device. This is the "list of commands"
    // we'll record this frame (draw, clear, etc.). We'll submit it in endFrame.
    state_->command_buffer = SDL_AcquireGPUCommandBuffer(state_->device);
    if (!state_->command_buffer) {
        // error
        return false;
    }
    return true;
}
```

**In plain English:** “Begin frame” means “acquire the command buffer we’ll record into.” All the following calls (beginRenderPass, drawIndexed, etc.) record into this buffer until we submit it in **endFrame**.

---

### Under the hood – beginRenderPass (backend_vulkan.cpp)

```cpp
bool VulkanBackend::beginRenderPass(const RenderPassDesc& desc) {
    // ...
    SDL_GPUTexture* swapchain_texture = nullptr;

    // 1) Get the next image from the swapchain (the image we'll draw into).
    //    This might wait for the previous frame to finish (vsync).
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            state_->command_buffer, state_->window,
            &swapchain_texture, nullptr, nullptr)) {
        return false;
    }

    // 2) Describe the "render target": this texture, clear it to this color,
    //    and store the result when we're done.
    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = swapchain_texture;
    color_target.clear_color = SDL_FColor{
        desc.color_attachment.clear_color.r,
        desc.color_attachment.clear_color.g,
        desc.color_attachment.clear_color.b,
        desc.color_attachment.clear_color.a,
    };
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;   // clear at start
    color_target.store_op = SDL_GPU_STOREOP_STORE;  // keep the result

    // 3) Start the render pass: "all draws until endRenderPass go into this image"
    state_->render_pass = SDL_BeginGPURenderPass(state_->command_buffer, &color_target, 1, nullptr);
    return true;
}
```

**In plain English:** We (1) acquire the **swapchain image** (the current “screen” texture), (2) say “clear it to your clear color and store the result,” (3) **begin the render pass** so all draws go into that image. So “begin render pass” = attach the window’s image as the draw target and clear it.

---

### Under the hood – endRenderPass and endFrame (backend_vulkan.cpp)

```cpp
void VulkanBackend::endRenderPass() {
    if (!state_ || !state_->render_pass) return;
    SDL_EndGPURenderPass(state_->render_pass);
    state_->render_pass = nullptr;
}

bool VulkanBackend::endFrame() {
    // ...
    endRenderPass();   // make sure the pass is ended

    // Submit the command buffer. The GPU will execute all the commands we recorded
    // (clear, bind pipeline, bind buffers, draw). When that's done, the image
    // is presented to the window (user sees it).
    if (!SDL_SubmitGPUCommandBuffer(state_->command_buffer)) {
        return false;
    }
    state_->command_buffer = nullptr;
    return true;
}
```

**In plain English:** **endRenderPass** ends the render pass (no more draws to that image). **endFrame** submits the command buffer so the GPU runs it and presents the result. So “end frame” = “GPU, run everything we recorded and show it.”

---

<a id="toc-example-draw"></a>
## 8. Example 5: Drawing (Pipeline, Buffers, One Draw)

**What we want:** Inside the render pass we say: use this pipeline (shaders + vertex layout), this vertex buffer, this index buffer, this uniform data (e.g. MVP matrix), and draw 36 indices (12 triangles).

---

### What you write (our API)

```cpp
// Inside the loop, after beginRenderPass and before endRenderPass:

renderer.setPipeline(pipeline);

Mat4 mvp = /* projection * view */;
renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));

renderer.setVertexBuffer(0, vertexBuffer, 0);
renderer.setIndexBuffer(indexBuffer, 0);

renderer.drawIndexed(36);   // 36 indices = 12 triangles
```

You **set** the pipeline and the buffers (and push the uniform); then **drawIndexed** means “draw 36 indices as triangles using whatever is currently bound.”

---

### Under the hood – setPipeline, setVertexBuffer, setIndexBuffer

These don’t record a draw yet; they **store** what we’ll use at the next **drawIndexed** (the backend “remembers” the current pipeline and buffers).

**Backend (backend_vulkan.cpp):**

```cpp
void VulkanBackend::setPipeline(const GraphicsPipeline& pipeline) {
    if (state_) state_->current_pipeline = static_cast<SDL_GPUGraphicsPipeline*>(pipeline.handle);
}

void VulkanBackend::setVertexBuffer(u32 slot, const Buffer& buffer, u64 offset) {
    if (!state_ || slot > 0) return;
    state_->current_vertex_buffer = static_cast<SDL_GPUBuffer*>(buffer.handle);
    state_->current_vertex_offset = static_cast<Uint32>(offset);
}

void VulkanBackend::setIndexBuffer(const Buffer& buffer, u64 offset) {
    if (state_) {
        state_->current_index_buffer = static_cast<SDL_GPUBuffer*>(buffer.handle);
        state_->current_index_offset = static_cast<Uint32>(offset);
    }
}
```

**In plain English:** We save the pipeline and buffer pointers (and offsets) in **state_**. When you call **drawIndexed**, we’ll bind these and then record the draw.

---

### Under the hood – pushVertexUniform

This **records** into the current command buffer: “make this data available to the vertex shader at the next draw.”

```cpp
void VulkanBackend::pushVertexUniform(u32 slot, const void* data, u64 size) {
    if (!state_ || !state_->command_buffer || !data || size == 0) return;
    SDL_PushGPUVertexUniformData(state_->command_buffer, slot, data, static_cast<Uint32>(size));
}
```

So your MVP matrix (or any small uniform) is “pushed” onto the command buffer and will be used when the GPU runs the next draw.

---

### Under the hood – drawIndexed (backend_vulkan.cpp)

This is where we **bind** the pipeline and buffers and **record the actual draw**.

```cpp
void VulkanBackend::drawIndexed(u32 index_count) {
    if (!state_ || !state_->render_pass || index_count == 0) return;

    // 1) Bind the pipeline (shaders + vertex layout + state)
    if (state_->current_pipeline)
        SDL_BindGPUGraphicsPipeline(state_->render_pass, state_->current_pipeline);

    // 2) Bind the vertex buffer(s)
    if (state_->current_vertex_buffer) {
        SDL_GPUBufferBinding binding{};
        binding.buffer = state_->current_vertex_buffer;
        binding.offset = state_->current_vertex_offset;
        SDL_BindGPUVertexBuffers(state_->render_pass, 0, &binding, 1);
    }

    // 3) Bind the index buffer
    if (state_->current_index_buffer) {
        SDL_GPUBufferBinding binding{};
        binding.buffer = state_->current_index_buffer;
        binding.offset = state_->current_index_offset;
        SDL_BindGPUIndexBuffer(state_->render_pass, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }

    // 4) Record the draw: "draw index_count indices, 1 instance, ..."
    SDL_DrawGPUIndexedPrimitives(state_->render_pass, index_count, 1, 0, 0, 0);
}
```

**In plain English:** We (1) bind the pipeline (so the GPU uses our shaders and vertex layout), (2) bind the vertex buffer, (3) bind the index buffer, (4) record “draw this many indices.” The **pushed uniform** was already recorded; the GPU will use it when it runs this draw. The actual execution happens when we **submit** the command buffer in **endFrame**.

---

<a id="toc-putting-together"></a>
## 9. Putting It All Together (One Frame, Start to Finish)

Here’s one frame from your code’s point of view and then from the GPU’s point of view.

**What you write:**

```cpp
while (renderer.pollEvents()) {
    if (!renderer.beginFrame()) break;
    if (!renderer.beginRenderPass(pass_desc)) break;

    renderer.setPipeline(pipeline);
    renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));
    renderer.setVertexBuffer(0, vertexBuffer, 0);
    renderer.setIndexBuffer(indexBuffer, 0);
    renderer.drawIndexed(36);

    renderer.endRenderPass();
    if (!renderer.endFrame()) break;
}
```

**What happens under the hood (in order):**

| Your call         | Renderer does              | Backend does                                                                 |
|-------------------|----------------------------|-------------------------------------------------------------------------------|
| **beginFrame()**  | Forwards to vulkan         | Acquires **command buffer** (the list we’ll fill this frame).                 |
| **beginRenderPass()** | Forwards to vulkan    | Acquires **swapchain image**, **begins render pass** (clear that image).      |
| **setPipeline**   | Forwards                   | Saves **current_pipeline** in state.                                         |
| **pushVertexUniform** | Forwards                | **Records** “push this data for the vertex shader” on the command buffer.     |
| **setVertexBuffer**   | Forwards                | Saves **current_vertex_buffer** in state.                                    |
| **setIndexBuffer**    | Forwards                | Saves **current_index_buffer** in state.                                    |
| **drawIndexed(36)**  | Forwards                | **Binds** pipeline, vertex buffer, index buffer; **records** “draw 36 indices”. |
| **endRenderPass()**  | Forwards                | **Ends** the render pass.                                                    |
| **endFrame()**       | Forwards                | **Submits** the command buffer → GPU runs it → image is **presented**.        |

So in one frame: we **record** “clear, bind pipeline and buffers, push uniform, draw 36 indices” into a command buffer, then **submit** it. The GPU runs those commands and draws your triangles; when it’s done, the new image is shown in the window.

---

## Where to go next

- **Run the tutorial:** From the project root: `zig build shaders` then `zig build run-tutorial`. That program uses every piece above.
- **Read the example:** `tutorial/main.cpp` has the same flow with comments.
- **Dive deeper:** `UNDER_THE_HOOD.md` has the full call chain and file layout; `BEGINNER_BACKENDS_GUIDE.md` compares our API with Vulkan, WebGPU, and OpenGL.

You now have both the **concepts** (what’s a buffer, a shader, a frame) and the **code** (what you call and what runs under the hood) in one place.
