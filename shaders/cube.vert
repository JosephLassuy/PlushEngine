#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 0) out vec3 vColor;

layout(set = 1, binding = 0) uniform Uniforms {
    mat4 uMVP;
};

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
