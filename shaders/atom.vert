#version 410 core
layout(location = 0) in vec3 aPos; // atom position in world space

uniform mat4 view;
uniform mat4 projection;

void main() {
    // World → eye → clip space
    gl_Position = view * projection * vec4(aPos, 1.0); // Not sure???
    gl_PointSize = 4.0; // pixels
}