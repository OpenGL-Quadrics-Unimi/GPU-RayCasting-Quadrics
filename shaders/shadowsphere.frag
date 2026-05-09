#version 410 core

// Billboard corner in [-1, 1]², passed through from the vertex shader.
in vec2 vCorner;

// Sphere billboard matches projected silhouette.
// Discarding the  corners to turn the square into a circle shadow.
void main()
{
    if (dot(vCorner, vCorner) > 1.0) discard;
}
