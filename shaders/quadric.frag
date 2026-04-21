#version 410 core
in vec3 vColor;
in vec2 vCorner;

out vec4 FragColor;

void main() {
    // Discard fragments outside the sphere's circle.
    if (dot(vCorner, vCorner) > 1.0) discard;

    FragColor = vec4(vColor, 1.0);
}