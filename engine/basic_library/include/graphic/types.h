#pragma once
#include <cstdint>

namespace gpu {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

/** Which low-level graphics API to use (Vulkan, WebGPU, etc.). */
enum class Backend {
    Vulkan,
    WebGPU,
    OpenGL,
};

/** Pixel formats for textures and render targets. */
enum class Format {
    Undefined,
    RGBA8_UNorm,
    BGRA8_UNorm,
    Depth24_Stencil8,
};

/** How a buffer will be used (can be combined with |). */
enum class BufferUsage : u32 {
    None    = 0,
    Vertex  = 1 << 0,
    Index   = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    CopySrc = 1 << 4,
    CopyDst = 1 << 5,
};
inline constexpr BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return BufferUsage(u32(a) | u32(b));
}

/** How a texture will be used. */
enum class TextureUsage : u32 {
    None         = 0,
    Sampled      = 1 << 0,
    RenderTarget = 1 << 1,
    DepthStencil = 1 << 2,
    CopySrc      = 1 << 3,
    CopyDst      = 1 << 4,
};

/** How vertices are connected (triangle list, strip, etc.). */
enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
};

/** Shader stage (vertex or fragment). */
enum class ShaderStage : u32 {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
};
inline constexpr ShaderStage operator|(ShaderStage a, ShaderStage b) {
    return ShaderStage(u32(a) | u32(b));
}

/** Width and height (e.g. window or texture size). */
struct Extent2D {
    u32 width  = 0;
    u32 height = 0;
};

/** RGBA color (0–1 float). */
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

/** Format of one vertex attribute (e.g. position = Float3). */
enum class VertexFormat {
    Float3,
    Float4,
};

} // namespace gpu
