/**
 * TUTORIAL: Drawing a 3D cube with the PlushEngine graphics API
 *
 * This file is a minimal, heavily commented example. It does the following:
 *   1. Create a renderer (window + Vulkan device)
 *   2. Create GPU resources: vertex buffer, index buffer, shaders, pipeline
 *   3. Each frame: clear the screen, draw the cube with a camera, present
 *
 * Run from project root after: zig build shaders && zig build run-tutorial
 */

#include <graphic.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

// =============================================================================
// MATH HELPERS (not part of the graphics API – just for the camera)
// =============================================================================
// We need a 4x4 matrix to pass to the vertex shader so the cube is drawn in 3D
// (projection * view). The GPU expects column-major order (OpenGL/Vulkan convention).

struct Mat4 {
    float m[16];  // column 0: m[0..3], column 1: m[4..7], etc.
};

static Mat4 perspective(float fovDeg, float aspect, float nearZ, float farZ) {
    float f = 1.0f / std::tan(fovDeg * 3.14159265f / 360.0f);
    float nf = 1.0f / (nearZ - farZ);
    Mat4 p{};
    p.m[0] = f / aspect;
    p.m[5] = f;
    p.m[10] = (farZ + nearZ) * nf;
    p.m[11] = -1.0f;
    p.m[14] = 2.0f * farZ * nearZ * nf;
    return p;
}

static Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
                   float atX, float atY, float atZ,
                   float upX, float upY, float upZ) {
    float fx = atX - eyeX, fy = atY - eyeY, fz = atZ - eyeZ;
    float len = std::sqrt(fx * fx + fy * fy + fz * fz);
    if (len > 0.0f) {
        fx /= len;
        fy /= len;
        fz /= len;
    }
    float ux = upX, uy = upY, uz = upZ;
    float sx = uy * fz - uz * fy, sy = uz * fx - ux * fz, sz = ux * fy - uy * fx;
    len = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (len > 0.0f) {
        sx /= len;
        sy /= len;
        sz /= len;
    }
    ux = fy * sz - fz * sy;
    uy = fz * sx - fx * sz;
    uz = fx * sy - fy * sx;
    Mat4 v{};
    v.m[0] = sx;
    v.m[4] = sy;
    v.m[8] = sz;
    v.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    v.m[1] = ux;
    v.m[5] = uy;
    v.m[9] = uz;
    v.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    v.m[2] = -fx;
    v.m[6] = -fy;
    v.m[10] = -fz;
    v.m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
    v.m[3] = 0;
    v.m[7] = 0;
    v.m[11] = 0;
    v.m[15] = 1.0f;
    return v;
}

static void mat4Mul(Mat4* out, const Mat4* a, const Mat4* b) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out->m[col * 4 + row] =
                a->m[0 * 4 + row] * b->m[col * 4 + 0] +
                a->m[1 * 4 + row] * b->m[col * 4 + 1] +
                a->m[2 * 4 + row] * b->m[col * 4 + 2] +
                a->m[3 * 4 + row] * b->m[col * 4 + 3];
        }
    }
}

// Load a binary file (e.g. compiled shader) into a byte vector.
static std::vector<std::uint8_t> loadFile(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

// =============================================================================
// CUBE GEOMETRY
// =============================================================================
// Each vertex: 3 floats position (x,y,z) + 3 floats color (r,g,b) = 24 bytes.
// The vertex shader receives these at location 0 (position) and location 1 (color).

struct Vertex {
    float x, y, z;
    float r, g, b;
};

// 8 corners of a cube centered at origin, side length 1. Each corner has a color.
static const Vertex cubeVertices[] = {
    {-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f},  // 0: red
    { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f},  // 1: green
    { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f},  // 2: blue
    {-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.0f},  // 3: yellow
    {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 1.0f},  // 4: cyan
    { 0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f},  // 5: magenta
    { 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 1.0f},  // 6: white
    {-0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 0.5f},  // 7: gray
};

// 12 triangles (2 per face) × 3 indices = 36 indices. Each triple is one triangle.
static const std::uint32_t cubeIndices[] = {
    4, 5, 6,  4, 6, 7,  // front face (z = +0.5)
    1, 0, 3,  1, 3, 2,  // back face (z = -0.5)
    5, 1, 2,  5, 2, 6,  // right
    0, 4, 7,  0, 7, 3,  // left
    3, 7, 6,  3, 6, 2,  // top
    0, 1, 5,  0, 5, 4,  // bottom
};

// =============================================================================
// MAIN
// =============================================================================

int main() {
    // --- STEP 1: Create and initialize the renderer ---
    // The renderer owns the window and the GPU "device". We never touch Vulkan
    // or SDL directly in this file; everything goes through gpu::Renderer.
    gpu::Renderer renderer;
    gpu::RendererDesc renderer_desc{};
    renderer_desc.device.backend = gpu::Backend::Vulkan;  // Use Vulkan on desktop
    renderer_desc.surface.window.title = "Tutorial – Cube";
    renderer_desc.surface.window.size = {1280, 720};

    if (!renderer.initialize(renderer_desc)) {
        std::cerr << "Failed to initialize renderer.\n";
        return 1;
    }

    // --- STEP 2: Create GPU resources (buffers, shaders, pipeline) ---
    // These live in GPU memory. We create them once and reuse every frame.
    gpu::Buffer vertexBuffer{};
    gpu::Buffer indexBuffer{};
    gpu::Shader vertShader{};
    gpu::Shader fragShader{};
    gpu::GraphicsPipeline pipeline{};
    bool haveCube = false;

    // Load compiled shaders (SPIR-V). We compile GLSL → SPIR-V with: zig build shaders
    std::vector<std::uint8_t> vertSpv = loadFile("shaders/cube.vert.spv");
    std::vector<std::uint8_t> fragSpv = loadFile("shaders/cube.frag.spv");

    if (!vertSpv.empty() && !fragSpv.empty()) {
        // 2a. Vertex buffer: holds the 8 cube vertices (position + color each).
        gpu::BufferDesc vbDesc{};
        vbDesc.size = sizeof(cubeVertices);
        vbDesc.usage = gpu::BufferUsage::Vertex;
        vertexBuffer = renderer.createBuffer(vbDesc);
        if (vertexBuffer.valid()) {
            renderer.writeBuffer(vertexBuffer, cubeVertices, sizeof(cubeVertices));
        }

        // 2b. Index buffer: holds the 36 indices (which vertices form triangles).
        if (vertexBuffer.valid()) {
            gpu::BufferDesc ibDesc{};
            ibDesc.size = sizeof(cubeIndices);
            ibDesc.usage = gpu::BufferUsage::Index;
            indexBuffer = renderer.createBuffer(ibDesc);
            if (indexBuffer.valid()) {
                renderer.writeBuffer(indexBuffer, cubeIndices, sizeof(cubeIndices));
            }
        }

        // 2c. Shaders: vertex shader (transforms position, passes color) and
        //     fragment shader (outputs color). We tell the API we have 1 uniform
        //     buffer in the vertex shader (the MVP matrix at set=1 binding=0).
        if (indexBuffer.valid()) {
            gpu::ShaderDesc vsDesc{};
            vsDesc.stage = gpu::ShaderStage::Vertex;
            vsDesc.code = vertSpv.data();
            vsDesc.code_size = vertSpv.size();
            vsDesc.entry_point = "main";
            vsDesc.num_uniform_buffers = 1;
            vertShader = renderer.createShader(vsDesc);

            gpu::ShaderDesc fsDesc{};
            fsDesc.stage = gpu::ShaderStage::Fragment;
            fsDesc.code = fragSpv.data();
            fsDesc.code_size = fragSpv.size();
            fsDesc.entry_point = "main";
            fragShader = renderer.createShader(fsDesc);
        }

        // 2d. Graphics pipeline: binds the two shaders and describes how to
        //     read the vertex buffer (stride 24 bytes, attribute 0 = position
        //     at offset 0, attribute 1 = color at offset 12).
        if (vertShader.valid() && fragShader.valid()) {
            gpu::VertexAttributeDesc attrs[2] = {
                {0, gpu::VertexFormat::Float3, 0},   // location 0: position (12 bytes)
                {1, gpu::VertexFormat::Float3, 12},  // location 1: color (12 bytes)
            };
            gpu::VertexLayoutDesc layout{};
            layout.stride = sizeof(Vertex);
            layout.attribute_count = 2;
            layout.attributes = attrs;

            gpu::GraphicsPipelineDesc pipeDesc{};
            pipeDesc.vertex_shader = vertShader;
            pipeDesc.fragment_shader = fragShader;
            pipeDesc.vertex_layout = layout;
            pipeDesc.primitive_type = gpu::PrimitiveTopology::TriangleList;
            pipeline = renderer.createGraphicsPipeline(pipeDesc);
            haveCube = pipeline.valid();
        }
    }

    if (!haveCube) {
        std::cout << "Cube not drawn. Run from project root after: zig build shaders\n";
    }

    // Describe the render pass: we render into the window (swapchain) and clear
    // it to this color at the start of the pass.
    gpu::RenderPassDesc pass_desc{};
    pass_desc.color_attachment.clear_color = {0.12f, 0.18f, 0.32f, 1.0f};

    // View matrix: camera at (1.2, 1, 1.8) looking at origin, Y up. Computed once.
    Mat4 view = lookAt(1.2f, 1.0f, 1.8f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    // --- STEP 3: Main loop – one iteration = one frame ---
    while (renderer.pollEvents()) {
        // 3a. Start a new frame. This acquires a command buffer from the GPU.
        if (!renderer.beginFrame()) break;

        // 3b. Start a render pass. This binds the window's back buffer as the
        //     render target and clears it to pass_desc.clear_color.
        if (!renderer.beginRenderPass(pass_desc)) break;

        if (haveCube) {
            // 3c. Build projection matrix from current window size (so aspect
            //     ratio is correct when the window is resized).
            gpu::Extent2D size = renderer.getSurfaceSize();
            float aspect = (size.height > 0)
                ? static_cast<float>(size.width) / static_cast<float>(size.height)
                : 1280.0f / 720.0f;
            Mat4 proj = perspective(60.0f, aspect, 0.1f, 100.0f);
            Mat4 mvp;
            mat4Mul(&mvp, &proj, &view);

            // 3d. Record draw commands (they are submitted at endFrame).
            renderer.setPipeline(pipeline);
            renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));  // slot 0 = MVP matrix
            renderer.setVertexBuffer(0, vertexBuffer, 0);
            renderer.setIndexBuffer(indexBuffer, 0);
            renderer.drawIndexed(36);  // 36 indices = 12 triangles
        }

        // 3e. End the render pass, then submit the command buffer and present.
        renderer.endRenderPass();
        if (!renderer.endFrame()) break;
    }

    // --- STEP 4: Cleanup ---
    if (pipeline.valid()) renderer.releaseGraphicsPipeline(pipeline);
    if (fragShader.valid()) renderer.releaseShader(fragShader);
    if (vertShader.valid()) renderer.releaseShader(vertShader);
    if (indexBuffer.valid()) renderer.releaseBuffer(indexBuffer);
    if (vertexBuffer.valid()) renderer.releaseBuffer(vertexBuffer);

    return 0;
}
