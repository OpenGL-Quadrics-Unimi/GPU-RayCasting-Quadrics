#version 410 core
layout(location = 0) in vec3 aPos;

// Normalized device coordinates passed to the fragment shader to reconstruct the ray direction
out vec2 vNDC;

void main() {
    vNDC = aPos.xy;
    gl_Position = vec4(aPos, 1.0);
}