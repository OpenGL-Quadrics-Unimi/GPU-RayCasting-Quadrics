#version 410 core

// Per-vertex (4 corners of a unit quad, values in {0,1})
layout(location = 0) in vec2  aCorner;

// Per-instance (divisor = 1): one atom
layout(location = 1) in vec3  aCenter;
layout(location = 2) in float aRadius;
layout(location = 3) in vec3  aColor;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;

out vec3  vCenterEye;
out float vRadius;
out vec3  vColor;

void main()
{
    // Move atom centre to eye space
    vec4 eyeCenter4 = uView * uModel * vec4(aCenter, 1.0);
    vec3 eyeCenter  = eyeCenter4.xyz;

    float r  = aRadius;
    float cx = eyeCenter.x, cy = eyeCenter.y, cz = eyeCenter.z;
    float fx = uProj[0][0], fy = uProj[1][1]; // focal lengths from projection matrix

    // Tight NDC bounding box via tangent lines from origin to sphere silhouette.
    // For the XZ plane: Q = cx²+cz²-r²; tangent NDC x = -fx*(cx*√Q - s*r*cz)/(cz*√Q + s*r*cx)
    // Same derivation for YZ plane. When Q ≤ 0 the camera is inside the sphere.
    float Qx = cx*cx + cz*cz - r*r;
    float Qy = cy*cy + cz*cz - r*r;

    float x_ndc, y_ndc;
    float s = aCorner.x; // ±1

    if (Qx > 0.0) {
        float sqQx = sqrt(Qx);
        x_ndc = -fx * (cx*sqQx - s*r*cz) / (cz*sqQx + s*r*cx);
    } else {
        x_ndc = aCorner.x; // camera inside sphere: cover full width
    }

    s = aCorner.y;
    if (Qy > 0.0) {
        float sqQy = sqrt(Qy);
        y_ndc = -fy * (cy*sqQy - s*r*cz) / (cz*sqQy + s*r*cy);
    } else {
        y_ndc = aCorner.y; // camera inside sphere: cover full height
    }

    // Use sphere centre depth; fragment shader overwrites gl_FragDepth with actual hit.
    vec4 clipCenter = uProj * vec4(eyeCenter, 1.0);
    float z_ndc     = clipCenter.z / clipCenter.w;

    gl_Position = vec4(x_ndc, y_ndc, z_ndc, 1.0);

    vCenterEye = eyeCenter;
    vRadius    = aRadius;
    vColor     = aColor;
}
