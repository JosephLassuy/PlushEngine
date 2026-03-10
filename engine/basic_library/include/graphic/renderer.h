#pragma once
#include "graphic/resources.h"
#include <memory>

namespace gpu {

/**
 * Main entry point for graphics: window, device, and recording draw commands.
 * Create one after startup; call initialize(), then create resources and draw each frame.
 */
class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(const RendererDesc& desc = {});
    bool pollEvents();
    bool shouldClose() const;

    bool beginFrame();
    bool beginRenderPass(const RenderPassDesc& desc);
    void endRenderPass();
    bool endFrame();

    const char* backendName() const;
    void shutdown();

    /** Current drawable size in pixels (for aspect ratio, etc.). */
    Extent2D getSurfaceSize() const;

    Buffer createBuffer(const BufferDesc& desc);
    bool writeBuffer(Buffer& buffer, const void* data, u64 size);
    void releaseBuffer(Buffer& buffer);

    Shader createShader(const ShaderDesc& desc);
    void releaseShader(Shader& shader);

    GraphicsPipeline createGraphicsPipeline(const GraphicsPipelineDesc& desc);
    void releaseGraphicsPipeline(GraphicsPipeline& pipeline);

    void setPipeline(const GraphicsPipeline& pipeline);
    void pushVertexUniform(u32 slot, const void* data, u64 size);
    void setVertexBuffer(u32 slot, const Buffer& buffer, u64 offset = 0);
    void setIndexBuffer(const Buffer& buffer, u64 offset = 0);
    void drawIndexed(u32 index_count);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gpu
