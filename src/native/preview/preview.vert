#version 450
layout(location = 0) in vec3 inPos;
layout(push_constant) uniform PC { mat4 mvp; mat4 view; } pc;
layout(location = 0) out vec3 vView;
layout(location = 1) out float vY;
void main() {
    vY = inPos.y;
    vView = (pc.view * vec4(inPos, 1.0)).xyz;
    gl_Position = pc.mvp * vec4(inPos, 1.0);
}
