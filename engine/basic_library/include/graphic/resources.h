#pragma once
#include "graphic/descriptors.h"

namespace gpu {

/** Opaque handle to a GPU buffer (vertex, index, uniform, etc.). */
struct Buffer {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};

/** Opaque handle to a compiled shader (vertex or fragment). */
struct Shader {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};

/** How to create a graphics pipeline (shaders + vertex layout + topology). */
struct GraphicsPipelineDesc {
    Shader vertex_shader{};
    Shader fragment_shader{};
    VertexLayoutDesc vertex_layout{};
    PrimitiveTopology primitive_type = PrimitiveTopology::TriangleList;
};

/** Opaque handle to a pipeline (shaders + fixed render state bound together). */
struct GraphicsPipeline {
    void* handle = nullptr;
    bool valid() const { return handle != nullptr; }
};

} // namespace gpu
