#include <basic_library/basic_library.h>
#include <graphic.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

// Column-major 4x4 matrix (OpenGL/Vulkan convention)
struct Mat4 {
    float m[16];
};

static Mat4 perspective(float fovDeg, float aspect, float nearZ, float farZ) {
    float f = 1.0f / std::tan(fovDeg * 3.14159265f / 360.0f);
    float nf = 1.0f / (nearZ - farZ);
    Mat4 p{};
    p.m[0] = f / aspect; p.m[5] = f; p.m[10] = (farZ + nearZ) * nf; p.m[11] = -1.0f;
    p.m[14] = 2.0f * farZ * nearZ * nf;
    return p;
}

static Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
                   float atX, float atY, float atZ,
                   float upX, float upY, float upZ) {
    float fx = atX - eyeX, fy = atY - eyeY, fz = atZ - eyeZ;
    float len = std::sqrt(fx*fx + fy*fy + fz*fz);
    if (len > 0.0f) { fx /= len; fy /= len; fz /= len; }
    float ux = upX, uy = upY, uz = upZ;
    float sx = uy*fz - uz*fy, sy = uz*fx - ux*fz, sz = ux*fy - uy*fx;
    len = std::sqrt(sx*sx + sy*sy + sz*sz);
    if (len > 0.0f) { sx /= len; sy /= len; sz /= len; }
    ux = fy*sz - fz*sy; uy = fz*sx - fx*sz; uz = fx*sy - fy*sx;
    Mat4 v{};
    v.m[0] = sx;  v.m[4] = sy;  v.m[8]  = sz;  v.m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    v.m[1] = ux;  v.m[5] = uy;  v.m[9]  = uz;  v.m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    v.m[2] = -fx; v.m[6] = -fy; v.m[10] = -fz; v.m[14] = (fx*eyeX + fy*eyeY + fz*eyeZ);
    v.m[3] = 0;   v.m[7] = 0;   v.m[11] = 0;   v.m[15] = 1.0f;
    return v;
}

static void mat4Mul(Mat4* out, const Mat4* a, const Mat4* b) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out->m[col*4+row] = a->m[0*4+row]*b->m[col*4+0] + a->m[1*4+row]*b->m[col*4+1]
                              + a->m[2*4+row]*b->m[col*4+2] + a->m[3*4+row]*b->m[col*4+3];
        }
    }
}

static std::vector<std::uint8_t> loadFile(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
    return buf;
}

// Cube: 8 vertices (position float3 + color float3), 36 indices
struct Vertex {
    float x, y, z;
    float r, g, b;
};
static const Vertex cubeVertices[] = {
    {-0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.0f},
    { 0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f},
    { 0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f},
    {-0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f},
    {-0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f},
    { 0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f},
    { 0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f},
    {-0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.5f},
};
static const std::uint32_t cubeIndices[] = {
    4, 5, 6,  4, 6, 7,
    1, 0, 3,  1, 3, 2,
    5, 1, 2,  5, 2, 6,
    0, 4, 7,  0, 7, 3,
    3, 7, 6,  3, 6, 2,
    0, 1, 5,  0, 5, 4,
};

int main() {
    basic_library::Print("Starting PlushEngine with a WebGPU-style renderer API on Vulkan");

    gpu::Renderer renderer;
    gpu::RendererDesc renderer_desc{};
    renderer_desc.device.backend = gpu::Backend::Vulkan;
    renderer_desc.surface.window.title = "PlushEngine";
    renderer_desc.surface.window.size = {1280, 720};

    if (!renderer.initialize(renderer_desc)) {
        return 1;
    }

    std::cout << "Created renderer backend: " << renderer.backendName() << '\n';

    gpu::Buffer vertexBuffer{};
    gpu::Buffer indexBuffer{};
    gpu::Shader vertShader{};
    gpu::Shader fragShader{};
    gpu::GraphicsPipeline pipeline{};
    bool haveCube = false;

    std::vector<std::uint8_t> vertSpv = loadFile("shaders/cube.vert.spv");
    std::vector<std::uint8_t> fragSpv = loadFile("shaders/cube.frag.spv");
    if (!vertSpv.empty() && !fragSpv.empty()) {
        gpu::BufferDesc vbDesc{};
        vbDesc.size = sizeof(cubeVertices);
        vbDesc.usage = gpu::BufferUsage::Vertex;
        vertexBuffer = renderer.createBuffer(vbDesc);
        if (vertexBuffer.valid() && renderer.writeBuffer(vertexBuffer, cubeVertices, sizeof(cubeVertices))) {
            gpu::BufferDesc ibDesc{};
            ibDesc.size = sizeof(cubeIndices);
            ibDesc.usage = gpu::BufferUsage::Index;
            indexBuffer = renderer.createBuffer(ibDesc);
            if (indexBuffer.valid() && renderer.writeBuffer(indexBuffer, cubeIndices, sizeof(cubeIndices))) {
                gpu::ShaderDesc vsDesc{};
                vsDesc.stage = gpu::ShaderStage::Vertex;
                vsDesc.code = vertSpv.data();
                vsDesc.code_size = vertSpv.size();
                vsDesc.entry_point = "main";
                vsDesc.num_uniform_buffers = 1;  // MVP at set=1 binding=0
                vertShader = renderer.createShader(vsDesc);
                gpu::ShaderDesc fsDesc{};
                fsDesc.stage = gpu::ShaderStage::Fragment;
                fsDesc.code = fragSpv.data();
                fsDesc.code_size = fragSpv.size();
                fsDesc.entry_point = "main";
                fragShader = renderer.createShader(fsDesc);
                if (vertShader.valid() && fragShader.valid()) {
                    gpu::VertexAttributeDesc attrs[2] = {
                        {0, gpu::VertexFormat::Float3, 0},
                        {1, gpu::VertexFormat::Float3, 12},
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
        }
    }
    if (!haveCube) {
        std::cout << "Cube not drawn (run 'zig build shaders' to compile shaders, then run from project root).\n";
    }

    gpu::RenderPassDesc pass_desc{};
    pass_desc.color_attachment.clear_color = {0.12f, 0.18f, 0.32f, 1.0f};

    Mat4 view = lookAt(1.2f, 1.0f, 1.8f,  0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f);

    while (renderer.pollEvents()) {
        if (!renderer.beginFrame()) break;
        if (!renderer.beginRenderPass(pass_desc)) break;

        if (haveCube) {
            gpu::Extent2D size = renderer.getSurfaceSize();
            float aspect = (size.height > 0) ? (float)size.width / (float)size.height : 1280.0f / 720.0f;
            Mat4 proj = perspective(60.0f, aspect, 0.1f, 100.0f);
            Mat4 mvp;
            mat4Mul(&mvp, &proj, &view);

            renderer.setPipeline(pipeline);
            renderer.pushVertexUniform(0, &mvp.m[0], sizeof(mvp.m));
            renderer.setVertexBuffer(0, vertexBuffer, 0);
            renderer.setIndexBuffer(indexBuffer, 0);
            renderer.drawIndexed(36);
        }

        renderer.endRenderPass();
        if (!renderer.endFrame()) break;
    }

    if (pipeline.valid()) renderer.releaseGraphicsPipeline(pipeline);
    if (fragShader.valid()) renderer.releaseShader(fragShader);
    if (vertShader.valid()) renderer.releaseShader(vertShader);
    if (indexBuffer.valid()) renderer.releaseBuffer(indexBuffer);
    if (vertexBuffer.valid()) renderer.releaseBuffer(vertexBuffer);
    return 0;
}
