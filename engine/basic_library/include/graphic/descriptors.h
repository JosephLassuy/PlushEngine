#pragma once
#include "graphic/types.h"

namespace gpu {

/** How to create the GPU device (backend, validation, etc.). */
struct DeviceDesc {
    Backend backend = Backend::Vulkan;
    bool enable_validation = false;
};

/** Window title, size, and options. */
struct WindowDesc {
    const char* title = "PlushEngine";
    Extent2D size{1280, 720};
    bool resizable = true;
    bool high_pixel_density = true;
};

/** Surface = window (or offscreen target). */
struct SurfaceDesc {
    WindowDesc window{};
};

/** Clear color and format for one color attachment. */
struct RenderPassColorAttachmentDesc {
    Format format = Format::BGRA8_UNorm;
    Color clear_color{0.12f, 0.18f, 0.32f, 1.0f};
};

/** What the render pass renders into (color, depth, etc.). */
struct RenderPassDesc {
    RenderPassColorAttachmentDesc color_attachment{};
};

/** Full renderer config (device + surface). */
struct RendererDesc {
    DeviceDesc device{};
    SurfaceDesc surface{};
};

/** Describes one vertex buffer (size, usage). */
struct BufferDesc {
    u64 size = 0;
    BufferUsage usage = BufferUsage::None;
};

/** Describes a shader (stage, SPIR-V code, entry point, uniform count). */
struct ShaderDesc {
    ShaderStage stage = ShaderStage::Vertex;
    const void* code = nullptr;
    u64 code_size = 0;
    const char* entry_point = "main";
    u32 num_uniform_buffers = 0;
};

/** One attribute in the vertex layout (location, format, byte offset). */
struct VertexAttributeDesc {
    u32 location = 0;
    VertexFormat format = VertexFormat::Float3;
    u32 offset = 0;
};

/** Stride and list of attributes for one vertex buffer. */
struct VertexLayoutDesc {
    u32 stride = 0;
    u32 attribute_count = 0;
    const VertexAttributeDesc* attributes = nullptr;
};

} // namespace gpu
