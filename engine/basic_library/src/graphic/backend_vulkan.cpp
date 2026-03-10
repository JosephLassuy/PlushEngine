#include "graphic/detail/backend_vulkan.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace gpu {

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

namespace {

void printSdlError(const char* message) {
    std::cerr << message << ": " << SDL_GetError() << '\n';
}

Uint32 toSdlBufferUsage(BufferUsage u) {
    Uint32 f = 0;
    u32 bits = static_cast<u32>(u);
    if ((bits & static_cast<u32>(BufferUsage::Vertex)) != 0) f |= SDL_GPU_BUFFERUSAGE_VERTEX;
    if ((bits & static_cast<u32>(BufferUsage::Index)) != 0) f |= SDL_GPU_BUFFERUSAGE_INDEX;
    return f;
}

SDL_GPUVertexElementFormat toSdlVertexFormat(VertexFormat vf) {
    switch (vf) {
        case VertexFormat::Float3: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case VertexFormat::Float4: return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    }
    return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
}

} // namespace

VulkanBackend::VulkanBackend() = default;

VulkanBackend::~VulkanBackend() {
    shutdown();
}

bool VulkanBackend::init(void* window, const RendererDesc& desc) {
    state_ = std::make_unique<State>();
    state_->window = static_cast<SDL_Window*>(window);
    state_->device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV,
        desc.device.enable_validation,
        "vulkan"
    );
    if (!state_->device) {
        printSdlError("Failed to create Vulkan GPU device");
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(state_->device, state_->window)) {
        printSdlError("Failed to claim the SDL window for the GPU device");
        SDL_DestroyGPUDevice(state_->device);
        state_->device = nullptr;
        state_.reset();
        return false;
    }
    return true;
}

void VulkanBackend::shutdown() {
    if (!state_) return;
    if (state_->render_pass) {
        SDL_EndGPURenderPass(state_->render_pass);
        state_->render_pass = nullptr;
    }
    if (state_->device && state_->window) {
        SDL_ReleaseWindowFromGPUDevice(state_->device, state_->window);
    }
    if (state_->device) {
        SDL_DestroyGPUDevice(state_->device);
        state_->device = nullptr;
    }
    state_->window = nullptr;
    state_.reset();
}

bool VulkanBackend::beginFrame() {
    if (!state_ || !state_->device) return false;
    if (state_->command_buffer) return true;
    state_->command_buffer = SDL_AcquireGPUCommandBuffer(state_->device);
    if (!state_->command_buffer) {
        printSdlError("Failed to acquire a GPU command buffer");
        return false;
    }
    return true;
}

bool VulkanBackend::beginRenderPass(const RenderPassDesc& desc) {
    if (!state_ || !state_->command_buffer) return false;
    if (state_->render_pass) return true;
    SDL_GPUTexture* swapchain_texture = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            state_->command_buffer, state_->window,
            &swapchain_texture, nullptr, nullptr)) {
        printSdlError("Failed to acquire the GPU swapchain texture");
        return false;
    }
    if (!swapchain_texture) return true;
    SDL_GPUColorTargetInfo color_target{};
    color_target.texture = swapchain_texture;
    color_target.clear_color = SDL_FColor{
        desc.color_attachment.clear_color.r,
        desc.color_attachment.clear_color.g,
        desc.color_attachment.clear_color.b,
        desc.color_attachment.clear_color.a,
    };
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    state_->render_pass = SDL_BeginGPURenderPass(state_->command_buffer, &color_target, 1, nullptr);
    if (!state_->render_pass) {
        printSdlError("Failed to begin the GPU render pass");
        return false;
    }
    return true;
}

void VulkanBackend::endRenderPass() {
    if (!state_ || !state_->render_pass) return;
    SDL_EndGPURenderPass(state_->render_pass);
    state_->render_pass = nullptr;
}

bool VulkanBackend::endFrame() {
    if (!state_ || !state_->command_buffer) return false;
    endRenderPass();
    if (!SDL_SubmitGPUCommandBuffer(state_->command_buffer)) {
        printSdlError("Failed to submit the GPU command buffer");
        state_->command_buffer = nullptr;
        return false;
    }
    state_->command_buffer = nullptr;
    return true;
}

const char* VulkanBackend::backendName() const {
    if (!state_ || !state_->device) return "uninitialized";
    const char* driver = SDL_GetGPUDeviceDriver(state_->device);
    return driver ? driver : "vulkan";
}

Extent2D VulkanBackend::getSurfaceSize() const {
    if (!state_ || !state_->window) return {0, 0};
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(state_->window, &w, &h)) return {0, 0};
    return {static_cast<u32>(w > 0 ? w : 0), static_cast<u32>(h > 0 ? h : 0)};
}

Buffer VulkanBackend::createBuffer(const BufferDesc& desc) {
    if (!state_ || !state_->device || desc.size == 0) return Buffer{};
    SDL_GPUBufferCreateInfo info{};
    info.usage = toSdlBufferUsage(desc.usage);
    info.size = static_cast<Uint32>(desc.size);
    info.props = 0;
    SDL_GPUBuffer* buf = SDL_CreateGPUBuffer(state_->device, &info);
    return Buffer{buf};
}

bool VulkanBackend::writeBuffer(Buffer& buffer, const void* data, u64 size) {
    if (!state_ || !state_->device || !buffer.handle || !data || size == 0) return false;
    SDL_GPUBuffer* buf = static_cast<SDL_GPUBuffer*>(buffer.handle);
    SDL_GPUTransferBufferCreateInfo tinfo{};
    tinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tinfo.size = static_cast<Uint32>(size);
    tinfo.props = 0;
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(state_->device, &tinfo);
    if (!transfer) return false;
    void* mapped = SDL_MapGPUTransferBuffer(state_->device, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(state_->device, transfer);
        return false;
    }
    std::memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(state_->device, transfer);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state_->device);
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(state_->device, transfer);
        return false;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src{};
    src.transfer_buffer = transfer;
    src.offset = 0;
    SDL_GPUBufferRegion dst{};
    dst.buffer = buf;
    dst.offset = 0;
    dst.size = static_cast<Uint32>(size);
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(state_->device, transfer);
    return true;
}

void VulkanBackend::releaseBuffer(Buffer& buffer) {
    if (!state_ || !buffer.handle) return;
    SDL_ReleaseGPUBuffer(state_->device, static_cast<SDL_GPUBuffer*>(buffer.handle));
    buffer.handle = nullptr;
}

Shader VulkanBackend::createShader(const ShaderDesc& desc) {
    if (!state_ || !state_->device || !desc.code || desc.code_size == 0) return Shader{};
    SDL_GPUShaderCreateInfo info{};
    info.code_size = desc.code_size;
    info.code = static_cast<const Uint8*>(desc.code);
    info.entrypoint = desc.entry_point ? desc.entry_point : "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = (desc.stage == ShaderStage::Vertex) ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    info.num_samplers = 0;
    info.num_storage_textures = 0;
    info.num_storage_buffers = 0;
    info.num_uniform_buffers = desc.num_uniform_buffers;
    info.props = 0;
    SDL_GPUShader* sh = SDL_CreateGPUShader(state_->device, &info);
    return Shader{sh};
}

void VulkanBackend::releaseShader(Shader& shader) {
    if (!state_ || !shader.handle) return;
    SDL_ReleaseGPUShader(state_->device, static_cast<SDL_GPUShader*>(shader.handle));
    shader.handle = nullptr;
}

GraphicsPipeline VulkanBackend::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
    if (!state_ || !state_->device || !desc.vertex_shader.valid() || !desc.fragment_shader.valid())
        return GraphicsPipeline{};
    std::vector<SDL_GPUVertexBufferDescription> vbDescs;
    std::vector<SDL_GPUVertexAttribute> attrs;
    if (desc.vertex_layout.attribute_count > 0 && desc.vertex_layout.attributes) {
        SDL_GPUVertexBufferDescription vb{};
        vb.slot = 0;
        vb.pitch = desc.vertex_layout.stride;
        vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb.instance_step_rate = 0;
        vbDescs.push_back(vb);
        for (u32 i = 0; i < desc.vertex_layout.attribute_count; ++i) {
            const VertexAttributeDesc& a = desc.vertex_layout.attributes[i];
            SDL_GPUVertexAttribute sa{};
            sa.location = a.location;
            sa.buffer_slot = 0;
            sa.format = toSdlVertexFormat(a.format);
            sa.offset = a.offset;
            attrs.push_back(sa);
        }
    }
    SDL_GPUVertexInputState vis{};
    vis.vertex_buffer_descriptions = vbDescs.empty() ? nullptr : vbDescs.data();
    vis.num_vertex_buffers = static_cast<Uint32>(vbDescs.size());
    vis.vertex_attributes = attrs.empty() ? nullptr : attrs.data();
    vis.num_vertex_attributes = static_cast<Uint32>(attrs.size());

    SDL_GPURasterizerState ras{};
    ras.fill_mode = SDL_GPU_FILLMODE_FILL;
    ras.cull_mode = SDL_GPU_CULLMODE_BACK;
    ras.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    ras.depth_bias_constant_factor = ras.depth_bias_clamp = ras.depth_bias_slope_factor = 0;
    ras.enable_depth_bias = false;
    ras.enable_depth_clip = true;
    ras.padding1 = ras.padding2 = 0;

    SDL_GPUMultisampleState ms{};
    ms.sample_count = SDL_GPU_SAMPLECOUNT_1;
    ms.sample_mask = 0;
    ms.enable_mask = false;
    ms.enable_alpha_to_coverage = false;
    ms.padding2 = ms.padding3 = 0;

    SDL_GPUStencilOpState stencilOp{};
    stencilOp.fail_op = stencilOp.pass_op = stencilOp.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
    stencilOp.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
    SDL_GPUDepthStencilState ds{};
    ds.compare_op = SDL_GPU_COMPAREOP_LESS;
    ds.back_stencil_state = ds.front_stencil_state = stencilOp;
    ds.compare_mask = ds.write_mask = 0xFF;
    ds.enable_depth_test = ds.enable_depth_write = ds.enable_stencil_test = false;
    ds.padding1 = ds.padding2 = ds.padding3 = 0;

    SDL_GPUColorTargetBlendState blend{};
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.color_write_mask = 0xF;
    blend.enable_blend = blend.enable_color_write_mask = false;
    blend.padding1 = blend.padding2 = 0;
    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    colorTarget.blend_state = blend;
    SDL_GPUGraphicsPipelineTargetInfo targetInfo{};
    targetInfo.color_target_descriptions = &colorTarget;
    targetInfo.num_color_targets = 1;
    targetInfo.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    targetInfo.has_depth_stencil_target = false;
    targetInfo.padding1 = targetInfo.padding2 = targetInfo.padding3 = 0;

    SDL_GPUGraphicsPipelineCreateInfo pinfo{};
    pinfo.vertex_shader = static_cast<SDL_GPUShader*>(desc.vertex_shader.handle);
    pinfo.fragment_shader = static_cast<SDL_GPUShader*>(desc.fragment_shader.handle);
    pinfo.vertex_input_state = vis;
    pinfo.primitive_type = (desc.primitive_type == PrimitiveTopology::TriangleStrip)
        ? SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP : SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pinfo.rasterizer_state = ras;
    pinfo.multisample_state = ms;
    pinfo.depth_stencil_state = ds;
    pinfo.target_info = targetInfo;
    pinfo.props = 0;
    SDL_GPUGraphicsPipeline* pipe = SDL_CreateGPUGraphicsPipeline(state_->device, &pinfo);
    return GraphicsPipeline{pipe};
}

void VulkanBackend::releaseGraphicsPipeline(GraphicsPipeline& pipeline) {
    if (!state_ || !pipeline.handle) return;
    SDL_ReleaseGPUGraphicsPipeline(state_->device, static_cast<SDL_GPUGraphicsPipeline*>(pipeline.handle));
    pipeline.handle = nullptr;
}

void VulkanBackend::setPipeline(const GraphicsPipeline& pipeline) {
    if (state_) state_->current_pipeline = static_cast<SDL_GPUGraphicsPipeline*>(pipeline.handle);
}

void VulkanBackend::pushVertexUniform(u32 slot, const void* data, u64 size) {
    if (!state_ || !state_->command_buffer || !data || size == 0) return;
    SDL_PushGPUVertexUniformData(state_->command_buffer, slot, data, static_cast<Uint32>(size));
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

void VulkanBackend::drawIndexed(u32 index_count) {
    if (!state_ || !state_->render_pass || index_count == 0) return;
    if (state_->current_pipeline)
        SDL_BindGPUGraphicsPipeline(state_->render_pass, state_->current_pipeline);
    if (state_->current_vertex_buffer) {
        SDL_GPUBufferBinding binding{};
        binding.buffer = state_->current_vertex_buffer;
        binding.offset = state_->current_vertex_offset;
        SDL_BindGPUVertexBuffers(state_->render_pass, 0, &binding, 1);
    }
    if (state_->current_index_buffer) {
        SDL_GPUBufferBinding binding{};
        binding.buffer = state_->current_index_buffer;
        binding.offset = state_->current_index_offset;
        SDL_BindGPUIndexBuffer(state_->render_pass, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }
    SDL_DrawGPUIndexedPrimitives(state_->render_pass, index_count, 1, 0, 0, 0);
}

} // namespace gpu
