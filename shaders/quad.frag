#version 410 core
out vec4 FragColor;

// Add the quadric's matrix that stays constant across all fragments as a uniform variable
uniform mat4 Quad;

void main() {
    FragColor = vec4(1.0, 0.5, 0.0, 1.0);
}