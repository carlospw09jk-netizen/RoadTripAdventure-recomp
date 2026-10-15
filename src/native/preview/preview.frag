#version 450
layout(location = 0) in vec3 vView;
layout(location = 1) in float vY;
layout(location = 0) out vec4 outColor;
void main() {
    vec3 n = normalize(cross(dFdx(vView), dFdy(vView)));
    float l = max(dot(n, normalize(vec3(0.4, 0.6, 1.0))), 0.0);
    vec3 tint = vec3(0.9, 0.8, 0.6);
    if (abs(vY) < 1e-4) tint = vec3(0.25, 0.45, 0.3);  // ground plane (y == 0)
    outColor = vec4(vec3(0.25 + 0.75 * l) * tint, 1.0);
}
