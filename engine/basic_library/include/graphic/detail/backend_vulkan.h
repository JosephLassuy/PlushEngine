#pragma once
#include "graphic/resources.h"
#include <memory>

namespace gpu {

/** Vulkan (SDL_GPU) backend implementation. Used only by Renderer. */
class VulkanBackend {
public:
    VulkanBackend();
    ~VulkanBackend();
    VulkanBackend(VulkanBackend&&) noexcept = default;
    VulkanBackend& operator=(VulkanBackend&&) noexcept = default;
    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;

    bool init(void* window, const RendererDesc& desc);
    void shutdown();

    bool beginFrame();
    bool beginRenderPass(const RenderPassDesc& desc);
    void endRenderPass();
    bool endFrame();
    const char* backendName() const;
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
    void setVertexBuffer(u32 slot, const Buffer& buffer, u64 offset);
    void setIndexBuffer(const Buffer& buffer, u64 offset);
    void drawIndexed(u32 index_count);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace gpu
