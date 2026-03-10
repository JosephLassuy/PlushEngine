#include "graphic/renderer.h"
#include "graphic/detail/backend_vulkan.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <memory>

namespace {

const char* backendDriverName(gpu::Backend backend) {
    switch (backend) {
        case gpu::Backend::Vulkan: return "vulkan";
        case gpu::Backend::WebGPU: return "webgpu";
        case gpu::Backend::OpenGL: return "opengl";
    }
    return "unknown";
}

void printSdlError(const char* message) {
    std::cerr << message << ": " << SDL_GetError() << '\n';
}

} // namespace

namespace gpu {

struct Renderer::Impl {
    Backend backend = Backend::Vulkan;
    bool sdl_initialized = false;
    bool should_close = false;
    SDL_Window* window = nullptr;
    std::unique_ptr<VulkanBackend> vulkan;
};

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}

Renderer::~Renderer() { shutdown(); }

Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

bool Renderer::initialize(const RendererDesc& desc) {
    if (!impl_) impl_ = std::make_unique<Impl>();
    impl_->backend = desc.device.backend;
    impl_->should_close = false;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printSdlError("Failed to initialize SDL");
        impl_->should_close = true;
        return false;
    }
    impl_->sdl_initialized = true;

    Uint64 window_flags = 0;
    if (desc.surface.window.resizable) window_flags |= SDL_WINDOW_RESIZABLE;
    if (desc.surface.window.high_pixel_density) window_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

    impl_->window = SDL_CreateWindow(
        desc.surface.window.title,
        static_cast<int>(desc.surface.window.size.width),
        static_cast<int>(desc.surface.window.size.height),
        window_flags
    );
    if (!impl_->window) {
        printSdlError("Failed to create an SDL window");
        shutdown();
        return false;
    }

    switch (desc.device.backend) {
        case Backend::Vulkan:
            impl_->vulkan = std::make_unique<VulkanBackend>();
            if (!impl_->vulkan->init(impl_->window, desc)) {
                impl_->vulkan.reset();
                shutdown();
                return false;
            }
            return true;
        case Backend::WebGPU:
        case Backend::OpenGL:
            std::cerr << "Backend '" << backendDriverName(desc.device.backend)
                      << "' is not implemented yet.\n";
            impl_->should_close = true;
            shutdown();
            return false;
    }
    return false;
}

bool Renderer::pollEvents() {
    if (!impl_ || impl_->should_close) return false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) impl_->should_close = true;
    }
    return !impl_->should_close;
}

bool Renderer::shouldClose() const {
    return !impl_ || impl_->should_close;
}

bool Renderer::beginFrame() {
    if (!impl_ || impl_->should_close) return false;
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->beginFrame();
    return false;
}

bool Renderer::beginRenderPass(const RenderPassDesc& desc) {
    if (!impl_) return false;
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->beginRenderPass(desc);
    return false;
}

void Renderer::endRenderPass() {
    if (!impl_) return;
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->endRenderPass();
}

bool Renderer::endFrame() {
    if (!impl_ || impl_->should_close) return false;
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->endFrame();
    return false;
}

const char* Renderer::backendName() const {
    if (!impl_) return "uninitialized";
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->backendName();
    if (impl_->backend == Backend::WebGPU) return "webgpu";
    if (impl_->backend == Backend::OpenGL) return "opengl";
    return "unknown";
}

void Renderer::shutdown() {
    if (!impl_) return;
    if (impl_->vulkan) {
        impl_->vulkan->shutdown();
        impl_->vulkan.reset();
    }
    if (impl_->window) {
        SDL_DestroyWindow(impl_->window);
        impl_->window = nullptr;
    }
    if (impl_->sdl_initialized) {
        SDL_Quit();
        impl_->sdl_initialized = false;
    }
    impl_->should_close = true;
}

Extent2D Renderer::getSurfaceSize() const {
    if (!impl_) return {0, 0};
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->getSurfaceSize();
    return {0, 0};
}

Buffer Renderer::createBuffer(const BufferDesc& desc) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->createBuffer(desc);
    return Buffer{};
}

bool Renderer::writeBuffer(Buffer& buffer, const void* data, u64 size) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->writeBuffer(buffer, data, size);
    return false;
}

void Renderer::releaseBuffer(Buffer& buffer) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->releaseBuffer(buffer);
    buffer.handle = nullptr;
}

Shader Renderer::createShader(const ShaderDesc& desc) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->createShader(desc);
    return Shader{};
}

void Renderer::releaseShader(Shader& shader) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->releaseShader(shader);
    shader.handle = nullptr;
}

GraphicsPipeline Renderer::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        return impl_->vulkan->createGraphicsPipeline(desc);
    return GraphicsPipeline{};
}

void Renderer::releaseGraphicsPipeline(GraphicsPipeline& pipeline) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->releaseGraphicsPipeline(pipeline);
    pipeline.handle = nullptr;
}

void Renderer::setPipeline(const GraphicsPipeline& pipeline) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->setPipeline(pipeline);
}

void Renderer::pushVertexUniform(u32 slot, const void* data, u64 size) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->pushVertexUniform(slot, data, size);
}

void Renderer::setVertexBuffer(u32 slot, const Buffer& buffer, u64 offset) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->setVertexBuffer(slot, buffer, offset);
}

void Renderer::setIndexBuffer(const Buffer& buffer, u64 offset) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->setIndexBuffer(buffer, offset);
}

void Renderer::drawIndexed(u32 index_count) {
    if (impl_->backend == Backend::Vulkan && impl_->vulkan)
        impl_->vulkan->drawIndexed(index_count);
}

} // namespace gpu
