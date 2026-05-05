#version 410 core

// Per-vertex (divisor = 0): billboard corner in [-1, 1]²
layout(location = 0) in vec2  aCorner;

// Per-instance (divisor = 1): one bond
layout(location = 1) in vec3  aPosA;
layout(location = 2) in vec3  aPosB;
layout(location = 3) in float aRadius;
layout(location = 4) in vec3  aColor;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;

out vec3  vPosAEye;
out vec3  vPosBEye;
out float vRadius;
out vec3  vColor;

void main()
{
    vec3 eyeA = (uView * uModel * vec4(aPosA, 1.0)).xyz;
    vec3 eyeB = (uView * uModel * vec4(aPosB, 1.0)).xyz;

    vec3 axis = eyeB - eyeA;
    vec3 mid  = 0.5 * (eyeA + eyeB);

    // Screen-perpendicular direction: cross of axis with the camera's -Z axis.
    vec3 side = cross(axis, vec3(0.0, 0.0, -1.0));
    if (dot(side, side) < 1e-6)
        side = cross(axis, vec3(1.0, 0.0, 0.0));
    side = normalize(side) * aRadius;

    // aCorner.x ∈ [-1,1]: along cylinder axis (A=-1, B=+1)
    // aCorner.y ∈ [-1,1]: across cylinder width (±side)
    vec3 cornerEye = mid
                   + aCorner.x * 0.5 * axis
                   + aCorner.y * side;

    gl_Position = uProj * vec4(cornerEye, 1.0);

    vPosAEye = eyeA;
    vPosBEye = eyeB;
    vRadius  = aRadius;
    vColor   = aColor;
}
