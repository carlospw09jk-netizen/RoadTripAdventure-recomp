#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(push_constant) uniform PC { mat4 mvp; mat4 view; vec4 fog; } pc;
layout(location = 0) out vec3 vView;
layout(location = 1) out vec3 vColor;
void main() {
    vColor = inColor;
    vView = (pc.view * vec4(inPos, 1.0)).xyz;
    gl_Position = pc.mvp * vec4(inPos, 1.0);
}
