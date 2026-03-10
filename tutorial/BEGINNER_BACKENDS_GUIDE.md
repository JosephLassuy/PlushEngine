# Beginner’s Guide: Examples, Usage, and How Each Backend Handles It

This guide assumes **you don’t know graphics APIs yet**. For each idea we show: **what you actually write** (our API), **what Vulkan does**, **what WebGPU would do**, and **what OpenGL would do**. Same goal, different “languages” the driver speaks.

---

## Table of Contents

1. [What’s Going On Here? (Big Picture)](#toc-big-picture)
2. [Starting Up: Window and GPU “Device”](#toc-startup)
3. [Buffers: Putting Data on the GPU](#toc-buffers)
4. [Shaders: Small Programs on the GPU](#toc-shaders)
5. [Graphics Pipeline: Shaders + How to Read Vertices](#toc-pipeline)
6. [The Frame Loop: One Image per Loop](#toc-frame-loop)
7. [Drawing: “Use This Pipeline and These Buffers, Draw”](#toc-drawing)
8. [Cleaning Up](#toc-cleanup)
9. [Quick Reference: Same Idea, Four Ways](#toc-quick-ref)

---

<a id="toc-big-picture"></a>
## 1. What’s Going On Here? (Big Picture)

**What you want:** draw a 3D cube (or anything) on the screen.

**The problem:** the code that talks to the GPU is different depending on **which graphics API** the system uses:

| API      | Where it runs        | Who uses it                    |
|----------|----------------------|--------------------------------|
| **Vulkan** | Desktop (Windows, Linux, etc.) | Our engine (currently)        |
| **WebGPU** | Browsers             | Future: same app in a browser  |
| **OpenGL** | Desktop, older code  | Alternative desktop backend   |

So we **don’t want you to write Vulkan/WebGPU/OpenGL directly**. Instead you use **one API** (the `gpu::Renderer` and friends). The engine then **translates** your calls into whatever the current backend needs (Vulkan, or later WebGPU/OpenGL).

- **Your code** = same no matter the backend.
- **Our API** = the functions and types you call (`createBuffer`, `drawIndexed`, etc.).
- **Backend** = the piece that turns those calls into Vulkan / WebGPU / OpenGL.

In this guide we show **your usage** first, then how **Vulkan**, **WebGPU**, and **OpenGL** each handle the same idea. That way you see both “what do I type?” and “what’s actually happening under the hood?”

---

<a id="toc-startup"></a>
## 2. Starting Up: Window and GPU “Device”

**What it is:** Before drawing anything you need (1) a **window** (the rectangle on screen) and (2) a **device** (the connection to the GPU). The device is what you use to create buffers, shaders, and to submit work.

---

### Our API – What You Write

```cpp
#include <graphic.h>

gpu::Renderer renderer;
gpu::RendererDesc desc{};
desc.device.backend = gpu::Backend::Vulkan;   // or WebGPU / OpenGL when we add them
desc.surface.window.title = "My Game";
desc.surface.window.size = {1280, 720};

if (!renderer.initialize(desc)) {
    // handle error
    return 1;
}
// From now on: renderer owns the window and the GPU device.
```

You create **one** `Renderer`, give it a description (backend + window title/size), and call **`initialize`**. After that, all other calls go through `renderer`.

---

### How Vulkan Handles It

When the backend is Vulkan, `initialize` does (conceptually):

1. **SDL** creates the window (OS window).
2. **Vulkan**: create a **VkInstance** (driver connection), pick a **VkPhysicalDevice** (the GPU), create a **VkDevice** (logical device), create a **VkSurfaceKHR** from the window, then create a **VkSwapchainKHR** (the chain of images that get shown in the window).

So “device” in Vulkan = that **VkDevice** (and the queue you use to submit work). The swapchain is what you’ll eventually draw into so the user sees the next frame. We use **SDL_GPU** in the engine, which does this setup for us; you never see VkInstance/VkDevice directly.

---

### How WebGPU Would Handle It

In a browser, there is no “our Renderer” yet; you’d use the **WebGPU** API directly. The same *idea* looks like:

1. Get a **GPUAdapter** (the GPU): `navigator.gpu.requestAdapter()`.
2. Get a **GPUDevice**: `adapter.requestDevice()`.
3. The “window” is the canvas; you get a **GPUCanvasContext** from the canvas and configure it (e.g. `context.configure({ device, format })`). The context gives you the texture to render into each frame.

So: **adapter** ≈ physical GPU, **device** ≈ logical device (like Vulkan’s VkDevice), **canvas context** ≈ swapchain + surface. Our engine would hide this behind `initialize(desc)` and pick the right backend (e.g. WebGPU in browser).

---

### How OpenGL Would Handle It

In **OpenGL** the model is older and more “global state”:

1. The window is still created by the OS (e.g. SDL). You then create an **OpenGL context** and make it “current” for that window (`SDL_GL_CreateContext` etc.).
2. There is no separate “device” object like in Vulkan/WebGPU. The **context** is the “device”: all OpenGL calls affect the current context. So “initialize” in an OpenGL backend would: create the window, create the GL context, make it current. After that, `glCreateBuffer`, `glUseProgram`, etc. all go to that context.

So: **one context per window**, and that context is the “device” in OpenGL terms.

---

<a id="toc-buffers"></a>
## 3. Buffers: Putting Data on the GPU

**What it is:** A **buffer** is a chunk of memory on the GPU. You use it for things like:

- **Vertex data** – positions and colors of the corners of your shapes.
- **Index data** – which vertices form triangles (so you don’t repeat the same vertex many times).

You **create** the buffer (size + how it’s used), **upload** data from the CPU with `writeBuffer`, then later **bind** it when drawing.

---

### Our API – What You Write

```cpp
// Describe the buffer: how big, and that it will hold vertex data
gpu::BufferDesc vbDesc{};
vbDesc.size = sizeof(myVertices);           // e.g. 8 vertices × 24 bytes each
vbDesc.usage = gpu::BufferUsage::Vertex;

gpu::Buffer vertexBuffer = renderer.createBuffer(vbDesc);
if (!vertexBuffer.valid()) { /* error */ }

// Upload your CPU data into the GPU buffer
renderer.writeBuffer(vertexBuffer, myVertices, sizeof(myVertices));

// Later (each frame or when you draw): you'll bind this buffer with setVertexBuffer(0, vertexBuffer, 0)
```

For an **index** buffer it’s the same pattern, but `usage = gpu::BufferUsage::Index` and you upload index data (e.g. 36 uint32s for 12 triangles).

---

### How Vulkan Handles It

When you call `createBuffer` and `writeBuffer`, the Vulkan backend (via SDL_GPU) does roughly:

1. **Create buffer:**  
   Allocate a **VkBuffer** with the right size and usage flags (e.g. `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT`). Vulkan also needs **memory** (VkDeviceMemory) bound to that buffer; the driver picks a heap that the GPU can read from.

2. **Upload (writeBuffer):**  
   The CPU can’t write directly into most GPU buffers. So we:
   - Create a **staging buffer** (host-visible memory),
   - Copy your data into the staging buffer (mapped memory),
   - Record a **copy command** (copy from staging → vertex buffer),
   - Submit that command so the GPU does the copy.

So “writeBuffer” in Vulkan terms = staging buffer + copy command + submit. When you draw, we bind the **final** vertex buffer (the one we copied into), not the staging one.

---

### How WebGPU Would Handle It

In WebGPU the idea is the same:

1. **Create buffer:**  
   `device.createBuffer({ size, usage: GPUBufferUsage.VERTEX })` – you get a **GPUBuffer**. Usage flags tell the driver it’s for vertices (or indices, etc.).

2. **Upload:**  
   You **map** the buffer (or use a staging buffer). In WebGPU you might do `buffer.getMappedRange()`, write into it, then `buffer.unmap()`. For uploads from CPU, there’s often a “copy buffer to buffer” on a command encoder. So again: get data into a buffer the GPU can use (either by mapping if allowed, or by copying from a staging buffer).

Our `writeBuffer` would do the same steps: create/use a temporary buffer, copy your data in, then copy to the final vertex buffer if needed.

---

### How OpenGL Handles It

In OpenGL:

1. **Create buffer:**  
   `glGenBuffers(1, &id)` then `glBindBuffer(GL_ARRAY_BUFFER, id)` and `glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW)`. So you often **create and upload in one go**: the buffer is created and filled when you call `glBufferData`. The “usage” (e.g. vertex vs index) is implied by which target you bind to: **GL_ARRAY_BUFFER** for vertices, **GL_ELEMENT_ARRAY_BUFFER** for indices.

2. **Upload:**  
   If you already created the buffer and want to update it later, you’d `glBufferSubData`. So OpenGL is more “direct”: you often upload in one shot at creation time, and the driver handles where the memory lives.

So our `createBuffer` + `writeBuffer` in an OpenGL backend would map to: `glGenBuffers`, `glBindBuffer(GL_ARRAY_BUFFER/GL_ELEMENT_ARRAY_BUFFER)`, `glBufferData` (and optionally `glBufferSubData` for updates).

---

<a id="toc-shaders"></a>
## 4. Shaders: Small Programs on the GPU

**What it is:** A **shader** is a small program that runs on the GPU. You need at least two to draw triangles:

- **Vertex shader** – runs **per vertex**. It gets one vertex (position, color, etc.) and can change it. It usually multiplies position by a matrix (so the cube moves with the camera) and passes color to the next stage.
- **Fragment shader** – runs **per pixel** (per “fragment”). It gets the color (and whatever else the vertex shader passed) and outputs the final color for that pixel.

We write shaders in **GLSL** (text), compile them to **SPIR-V** (binary), and the engine loads that binary and gives it to the backend.

---

### Our API – What You Write

```cpp
// Load the compiled shader bytes (you ran: zig build shaders)
std::vector<std::uint8_t> vertSpv = loadFile("shaders/cube.vert.spv");
std::vector<std::uint8_t> fragSpv = loadFile("shaders/cube.frag.spv");

// Vertex shader
gpu::ShaderDesc vsDesc{};
vsDesc.stage = gpu::ShaderStage::Vertex;
vsDesc.code = vertSpv.data();
vsDesc.code_size = vertSpv.size();
vsDesc.entry_point = "main";
vsDesc.num_uniform_buffers = 1;   // we have one uniform (e.g. MVP matrix)

gpu::Shader vertShader = renderer.createShader(vsDesc);

// Fragment shader (no uniforms in this example)
gpu::ShaderDesc fsDesc{};
fsDesc.stage = gpu::ShaderStage::Fragment;
fsDesc.code = fragSpv.data();
fsDesc.code_size = fragSpv.size();
fsDesc.entry_point = "main";

gpu::Shader fragShader = renderer.createShader(fsDesc);
```

You don’t compile GLSL in C++; you compile offline (`glslangValidator` or `zig build shaders`) and load the resulting `.spv` file. The engine only ever sees the binary.

---

### How Vulkan Handles It

In Vulkan:

1. You have **SPIR-V** bytecode (which we do).
2. You create a **VkShaderModule** from that bytecode: `vkCreateShaderModule(device, &createInfo, nullptr, &module)`. The create info points at the SPIR-V code and size.
3. When you build a **pipeline** (next section), you attach these modules to the pipeline; Vulkan then compiles/links them for the specific GPU. So “createShader” in our backend = create **VkShaderModule**; the pipeline later uses these modules.

Vulkan **only** accepts SPIR-V for shaders (no GLSL at runtime). That’s why we compile GLSL → SPIR-V offline.

---

### How WebGPU Would Handle It

WebGPU also uses a binary shader format: **WGSL** is the human‑readable language, and the browser compiles it to an internal format. You typically do:

1. **Create shader module:**  
   `device.createShaderModule({ code: wgslString })` – you can pass WGSL source and the browser compiles it. There is also work to allow precompiled (SPIR-V–like) modules.
2. The module is then used when creating a **render pipeline** (same idea as our pipeline: vertex + fragment shader bound together).

So “createShader” in a WebGPU backend could: either compile WGSL (if we shipped WGSL) or, if we stick to SPIR-V, we’d need a way to get SPIR-V into the browser (e.g. via extension or translated to WGSL). The **concept** is the same: one module per shader stage, then attach to pipeline.

---

### How OpenGL Handles It

In OpenGL the classic approach is:

1. **Compile per stage:**  
   Create a shader object with `glCreateShader(GL_VERTEX_SHADER)` or `GL_FRAGMENT_SHADER`, then `glShaderSource` (with **GLSL source as a string**), then `glCompileShader`. So OpenGL compiles **GLSL at runtime**.
2. **Link program:**  
   You attach both shaders to a **program** with `glAttachShader(program, shader)` and then `glLinkProgram(program)`. The “pipeline” in OpenGL terms is roughly that **program** (vertex + fragment linked together).

So our “createShader” in an OpenGL backend would: either compile GLSL ourselves and pass the source, or compile GLSL to something and feed that (OpenGL doesn’t use SPIR-V natively). Then “createGraphicsPipeline” would create a **program**, attach both shaders, and link. So: **two shader objects + one program** = our two Shaders + one GraphicsPipeline.

---

<a id="toc-pipeline"></a>
## 5. Graphics Pipeline: Shaders + How to Read Vertices

**What it is:** The **pipeline** is a fixed bundle of state the GPU uses for drawing:

- Which **vertex** and **fragment** shaders to run.
- **Vertex layout**: how to read the vertex buffer (stride, and each “attribute” – position at offset 0, color at offset 12, etc.).
- Other fixed state: triangle list vs strip, culling, blending, etc.

You create **one pipeline** per “way of drawing” (e.g. one for the cube, one for UI). Then each frame you **bind** that pipeline and draw.

---

### Our API – What You Write

```cpp
// We already have vertShader and fragShader from the previous section.

gpu::VertexAttributeDesc attrs[2] = {
    {0, gpu::VertexFormat::Float3, 0},   // location 0: 3 floats, at byte offset 0 (position)
    {1, gpu::VertexFormat::Float3, 12}, // location 1: 3 floats, at byte offset 12 (color)
};
gpu::VertexLayoutDesc layout{};
layout.stride = 24;              // bytes per vertex
layout.attribute_count = 2;
layout.attributes = attrs;

gpu::GraphicsPipelineDesc pipeDesc{};
pipeDesc.vertex_shader = vertShader;
pipeDesc.fragment_shader = fragShader;
pipeDesc.vertex_layout = layout;
pipeDesc.primitive_type = gpu::PrimitiveTopology::TriangleList;

gpu::GraphicsPipeline pipeline = renderer.createGraphicsPipeline(pipeDesc);
```

So you’re saying: “When we draw, use these two shaders; and when reading the vertex buffer, use 24 bytes per vertex, with attribute 0 at offset 0 (Float3) and attribute 1 at offset 12 (Float3).” That must match your vertex shader (`layout(location=0) in vec3 aPos; layout(location=1) in vec3 aColor;`).

---

### How Vulkan Handles It

In Vulkan a **pipeline** is one big object (**VkPipeline**) that encodes almost everything:

1. **Shader stages:**  
   You reference the **VkShaderModule**s and specify the entry point and stage (vertex/fragment).
2. **Vertex input:**  
   You describe binding(s) (which buffer slot, stride) and attributes (location, format, offset). So our `VertexLayoutDesc` is turned into **VkVertexInputBindingDescription** and **VkVertexInputAttributeDescription**.
3. **Fixed state:**  
   Rasterization (cull mode, etc.), multisampling, depth/stencil, color blend – all part of the pipeline.

So **createGraphicsPipeline** in the Vulkan backend builds a **VkGraphicsPipelineCreateInfo** (with all the above), then calls **vkCreateGraphicsPipelines**. That’s a heavy object but very efficient to bind at draw time.

---

### How WebGPU Would Handle It

In WebGPU you create a **GPURenderPipeline**:

1. You pass **vertex** and **fragment** modules (from createShaderModule).
2. You describe **vertex buffers** and their attributes (same idea as our layout: stride, attributes with location/format/offset).
3. You can set primitive topology, etc.

So our `GraphicsPipelineDesc` (shaders + vertex layout + primitive type) maps directly to the WebGPU pipeline descriptor. “createGraphicsPipeline” would call **device.createRenderPipeline(descriptor)**.

---

### How OpenGL Would Handle It

In OpenGL there is no single “pipeline” object like Vulkan/WebGPU. Instead:

1. You have a **program** (linked vertex + fragment shaders).
2. **Vertex layout** is done at draw time with **vertex attribute arrays**: you call `glVertexAttribPointer` (and enable the attribute) to say “location 0 comes from this buffer, this stride, this offset, Float3.” So the “layout” we describe in the pipeline is applied when we set up the VAO (Vertex Array Object) or when we bind the buffer and set attrib pointers.

So an OpenGL backend would: when we **createGraphicsPipeline**, create a **program** (from the two shaders) and store the layout (stride, attributes). When we **setPipeline** (and set vertex buffer), we’d bind the program and configure the vertex attributes from that stored layout. The “pipeline” in our API is then **program + how we set up the VAO/attribs** from the layout.

---

<a id="toc-frame-loop"></a>
## 6. The Frame Loop: One Image per Loop

**What it is:** Drawing happens **per frame**. One iteration of the game loop = one frame. For that frame you:

1. **Begin frame** – get a command list (and/or the next swapchain image).
2. **Begin render pass** – “we’re going to draw into the screen; clear it to this color.”
3. Do all your draws (set pipeline, bind buffers, push uniforms, draw).
4. **End render pass** – “done drawing to the screen.”
5. **End frame** – submit the commands and present the image to the window.

So: **beginFrame → beginRenderPass → … draws … → endRenderPass → endFrame**.

---

### Our API – What You Write

```cpp
gpu::RenderPassDesc pass_desc{};
pass_desc.color_attachment.clear_color = {0.12f, 0.18f, 0.32f, 1.0f};  // dark blue

while (renderer.pollEvents()) {

    if (!renderer.beginFrame()) break;

    if (!renderer.beginRenderPass(pass_desc)) break;

    // ... set pipeline, buffers, uniforms, then drawIndexed(...) ...

    renderer.endRenderPass();

    if (!renderer.endFrame()) break;
}
```

- **pollEvents** – process window events (e.g. close button); returns false when the app should exit.
- **beginFrame** – start recording commands for this frame.
- **beginRenderPass** – attach the “screen” as the render target and clear it.
- **endRenderPass** – stop rendering to that target.
- **endFrame** – submit the recorded commands so the GPU runs them and the image is presented.

---

### How Vulkan Handles It

- **beginFrame:**  
  We **acquire a command buffer** (e.g. from a pool). In Vulkan you record all work into **VkCommandBuffer**s, then submit them. So “beginFrame” = get a command buffer for this frame.

- **beginRenderPass:**  
  We **acquire the next swapchain image** (the texture we’ll draw into). Then we **begin a render pass** (**vkCmdBeginRenderPass**): we say “this command buffer is now rendering into this image, clear it, and store the result.” All draws until endRenderPass go into that image.

- **endRenderPass:**  
  **vkCmdEndRenderPass** – no more draws to that image.

- **endFrame:**  
  **Submit** the command buffer to the queue (**vkQueueSubmit**). The GPU executes it. Then we **present** the swapchain image (**vkQueuePresentKHR**) so the window shows it.

So the “frame” in Vulkan = acquire command buffer → record render pass + draws → submit → present.

---

### How WebGPU Would Handle It

- **beginFrame:**  
  You get a **GPUTexture** from the canvas context (the current back buffer): e.g. `context.getCurrentTexture()`. You might also get a **GPUCommandEncoder** (the thing you record commands into).

- **beginRenderPass:**  
  You create a **GPURenderPassEncoder** from the command encoder: `encoder.beginRenderPass({ colorAttachments: [{ view: texture.createView(), loadOp: 'clear', clearValue: [0.12, 0.18, 0.32, 1], storeOp: 'store' }] })`. So “begin render pass” = start a render pass that draws into that texture, with a clear.

- **endRenderPass:**  
  `renderPassEncoder.end()`.

- **endFrame:**  
  You **finish** the command encoder (`encoder.finish()`), then **submit** the resulting command buffer to the queue (`queue.submit([commandBuffer])`). The canvas context will present when the GPU is done (often implicit). So same idea: record into an encoder, finish, submit.

---

### How OpenGL Handles It

OpenGL doesn’t have explicit “frame” or “render pass” objects. It’s more implicit:

- **beginFrame:**  
  There’s no direct equivalent. You might “clear” and start drawing. Swap buffers happens at the end (see endFrame).

- **beginRenderPass:**  
  You **bind the default framebuffer** (the window) and **clear** it: `glBindFramebuffer(GL_FRAMEBUFFER, 0)`, `glClearColor(...)`, `glClear(GL_COLOR_BUFFER_BIT)`. So “begin render pass” = bind window as target + clear.

- **endRenderPass:**  
  No explicit call; you just stop drawing to the screen (or bind another framebuffer).

- **endFrame:**  
  **Swap buffers:** `SDL_GL_SwapWindow(window)` (or `eglSwapBuffers` etc.). That presents the back buffer to the window. So “end frame” in OpenGL = swap buffers. There’s no separate “command buffer submit” – OpenGL commands often execute in order as you call them (or get batched by the driver).

So the same **logical** flow (clear → draw → present) exists in OpenGL, but the API is “bind target, clear, draw, swap” rather than “begin frame, begin pass, draw, end pass, end frame.”

---

<a id="toc-drawing"></a>
## 7. Drawing: “Use This Pipeline and These Buffers, Draw”

**What it is:** Once you’re inside a render pass, you **set** the pipeline and the buffers, **push** any per-draw data (e.g. MVP matrix), and then say **“draw this many triangles”** (by index count). The GPU then runs your vertex shader for each vertex and your fragment shader for each pixel.

---

### Our API – What You Write

```cpp
// Inside the render pass, after beginRenderPass and before endRenderPass:

renderer.setPipeline(pipeline);

// Push the MVP matrix so the vertex shader can use it (e.g. at set=1 binding=0)
Mat4 mvp = ...;  // compute projection * view * model
renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));

renderer.setVertexBuffer(0, vertexBuffer, 0);
renderer.setIndexBuffer(indexBuffer, 0);

renderer.drawIndexed(36);   // 36 indices = 12 triangles (a cube)
```

So you’re saying: “Use this pipeline, this uniform data, this vertex buffer, this index buffer; now draw 36 indices (as triangles).”

---

### How Vulkan Handles It

When you call these, the Vulkan backend records into the current command buffer (inside the current render pass):

- **setPipeline:**  
  Store the pipeline; at **drawIndexed** we call **vkCmdBindPipeline** with the stored **VkPipeline** (graphics).

- **pushVertexUniform:**  
  We record **vkCmdPushConstants** (or a descriptor set update, depending on how we map “slot 0”) so the next draw sees this data. Our current backend uses SDL_GPU’s **SDL_PushGPUVertexUniformData**, which ends up as push constants or a small uniform buffer bound for that draw.

- **setVertexBuffer / setIndexBuffer:**  
  We store the buffer and offset. At **drawIndexed** we call **vkCmdBindVertexBuffers** and **vkCmdBindIndexBuffer** with those.

- **drawIndexed(36):**  
  We **bind** pipeline, vertex buffer(s), index buffer (if any), then **vkCmdDrawIndexed(36, 1, 0, 0, 0)** – 36 indices, 1 instance, first index 0, vertex offset 0, first instance 0.

So one `drawIndexed` in our API = a series of Vulkan bind commands + one draw call.

---

### How WebGPU Would Handle It

- **setPipeline:**  
  **renderPassEncoder.setPipeline(gpuRenderPipeline)** – same idea.

- **pushVertexUniform:**  
  You’d set a **bind group** (descriptor set) or use **setBindGroup** with a buffer that holds the uniform. Or you could use **setPushConstants** if the backend supports it. So “pushVertexUniform” would map to setting the right bind group / push constants for the vertex stage.

- **setVertexBuffer / setIndexBuffer:**  
  **renderPassEncoder.setVertexBuffer(0, buffer, offset)** and **renderPassEncoder.setIndexBuffer(buffer, offset)** (or similar – WebGPU has index format). Same idea as Vulkan.

- **drawIndexed(36):**  
  **renderPassEncoder.drawIndexed(36, 1, 0, 0, 0)** – same parameters as Vulkan.

So the **mapping is almost 1:1** between our API and WebGPU for the draw path.

---

### How OpenGL Would Handle It

- **setPipeline:**  
  **glUseProgram(program)** – make that program active. Then set up vertex attributes from the pipeline’s layout (VAO or glVertexAttribPointer).

- **pushVertexUniform:**  
  Get the uniform location: **glGetUniformLocation(program, "uMVP")**, then **glUniformMatrix4fv(loc, 1, GL_FALSE, mvp)**. So “pushVertexUniform” = set the uniform(s) on the current program.

- **setVertexBuffer / setIndexBuffer:**  
  **glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer)** and **glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer)**. Then **glVertexAttribPointer** (and enable) so the program reads from the right places. OpenGL often uses a **VAO** (Vertex Array Object) that stores the attrib setup; we’d bind that VAO when we set the pipeline or the vertex buffer.

- **drawIndexed(36):**  
  **glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0)** – draw 36 indices as triangles from the bound index buffer.

So the same logical steps: use program, set uniforms, bind buffers, set attrib pointers, draw indexed.

---

<a id="toc-cleanup"></a>
## 8. Cleaning Up

**What it is:** When you’re done (e.g. app exit), you **release** buffers, shaders, and pipelines, then **shutdown** the renderer so the window and device are destroyed.

---

### Our API – What You Write

```cpp
if (pipeline.valid())   renderer.releaseGraphicsPipeline(pipeline);
if (fragShader.valid()) renderer.releaseShader(fragShader);
if (vertShader.valid()) renderer.releaseShader(vertShader);
if (indexBuffer.valid()) renderer.releaseBuffer(indexBuffer);
if (vertexBuffer.valid()) renderer.releaseBuffer(vertexBuffer);

renderer.shutdown();   // or just let the Renderer destructor run
```

Order doesn’t have to be strict for this simple case, but we release pipelines before shaders (pipeline uses shaders) and buffers before shutting down the device.

---

### How Vulkan / WebGPU / OpenGL Handle It

- **Vulkan:**  
  **releaseBuffer** = destroy **VkBuffer** and free memory. **releaseShader** = destroy **VkShaderModule**. **releaseGraphicsPipeline** = destroy **VkPipeline**. **shutdown** = destroy swapchain, device, surface, instance (and close the window via SDL).

- **WebGPU:**  
  Buffers, shaders, and pipelines are **GPUObject**s; when you’re done you don’t use them anymore and the browser can garbage‑collect. Our **release** would just drop our handle; **shutdown** would release the device and canvas context.

- **OpenGL:**  
  **releaseBuffer** = **glDeleteBuffers**. **releaseShader** = **glDeleteShader** (and if the program is only used by one pipeline, we might delete the program too). **releaseGraphicsPipeline** = **glDeleteProgram**. **shutdown** = destroy the GL context and the window.

So in all backends we’re just releasing the underlying API objects and then tearing down the device/window.

---

<a id="toc-quick-ref"></a>
## 9. Quick Reference: Same Idea, Four Ways

| What you want to do      | Our API (what you write)        | Vulkan (under the hood)              | WebGPU (if we had it)        | OpenGL (if we had it)           |
|--------------------------|----------------------------------|--------------------------------------|------------------------------|----------------------------------|
| Start: window + device   | `renderer.initialize(desc)`      | Instance, device, surface, swapchain | Adapter, device, canvas ctx  | Window + GL context              |
| GPU buffer               | `createBuffer(desc)`             | VkBuffer + memory                    | GPUBuffer                     | glGenBuffers + glBufferData      |
| Upload CPU → GPU         | `writeBuffer(buf, data, size)`   | Staging buffer + copy cmd + submit   | Map or copy buffer            | glBufferData / glBufferSubData   |
| Shader (vertex/fragment) | `createShader(desc)`             | VkShaderModule                      | createShaderModule            | glCreateShader + glCompileShader |
| Pipeline                 | `createGraphicsPipeline(desc)`  | VkPipeline (full state)             | createRenderPipeline          | Program + VAO/attrib setup       |
| Start recording a frame  | `beginFrame()`                   | Acquire command buffer               | getCurrentTexture, encoder     | (implicit)                      |
| Start drawing to screen  | `beginRenderPass(pass_desc)`     | Acquire swap image, BeginRenderPass  | beginRenderPass               | glBindFramebuffer(0), glClear    |
| Use pipeline             | `setPipeline(pipeline)`         | vkCmdBindPipeline                    | setPipeline                   | glUseProgram (+ VAO)             |
| Set per-draw data        | `pushVertexUniform(slot, ...)`  | Push constants / descriptor          | setBindGroup / setPushConstants | glUniform*                       |
| Bind vertex/index buf    | `setVertexBuffer` / `setIndexBuffer` | vkCmdBindVertexBuffers / IndexBuffer | setVertexBuffer / setIndexBuffer | glBindBuffer + glVertexAttribPointer |
| Draw triangles           | `drawIndexed(36)`                | vkCmdDrawIndexed                     | drawIndexed                    | glDrawElements                   |
| Stop drawing to screen   | `endRenderPass()`                | EndRenderPass                        | end() on render pass          | (implicit)                      |
| Submit and show frame    | `endFrame()`                     | Submit command buffer, Present       | encoder.finish(), queue.submit | Swap buffers                    |
| Release resource         | `releaseBuffer` / `releaseShader` / `releaseGraphicsPipeline` | Destroy Vk* objects   | Drop references                 | glDeleteBuffers / glDeleteProgram |
| Tear down                | `shutdown()`                     | Destroy device, instance, window     | Release device, context        | Destroy context, window         |

---

## What to Do Next

- **Run the tutorial:** From project root: `zig build shaders` then `zig build run-tutorial`. That runs the cube example that uses every concept above.
- **Read the code:** Open `tutorial/main.cpp` and follow the comments; they match the “Our API” examples in this guide.
- **Dive deeper:** Use **WALKTHROUGH.md** for a step‑by‑step walk of the same example and the Vulkan backend code (SDL_GPU and how each of our calls turns into Vulkan).

If something is still unclear, treat this doc as a map: find the **concept** (e.g. “buffers”), look at **our usage** first, then compare with the backend you care about (Vulkan, WebGPU, or OpenGL).
