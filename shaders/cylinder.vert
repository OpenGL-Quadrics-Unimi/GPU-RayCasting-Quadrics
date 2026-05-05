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
    // Transform bond endpoints to eye space.
    vec3 eyeA = (uView * uModel * vec4(aPosA, 1.0)).xyz;
    vec3 eyeB = (uView * uModel * vec4(aPosB, 1.0)).xyz;

    float r  = aRadius;
    float fx = uProj[0][0];   // focal length X
    float fy = uProj[1][1];   // focal length Y

    // --- Tight NDC bounding box (replaces the old eye-space billboard) ---
    //
    // WHY: the old approach built a quad by moving the midpoint left/right by
    // a fixed 'side' vector in eye space. That is fast but slightly wrong: as
    // the camera gets close to a bond, the perspective distortion means the
    // real silhouette is wider than the billboard, so cylinder edges get clipped.
    //
    // HOW: we use the same Sigg 2006 formula that quadric.vert uses for spheres.
    // That formula finds, for a sphere at eye-space (cx, ?, cz) with radius r,
    // the two NDC x-coordinates where tangent rays from the camera touch the
    // silhouette:
    //
    //   Q  = cx^2 + cz^2 - r^2          (Q > 0 means camera is outside the sphere)
    //   x1 = -fx * (cx*sqrt(Q) - r*cz) / (cz*sqrt(Q) + r*cx)   (s = +1)
    //   x2 = -fx * (cx*sqrt(Q) + r*cz) / (cz*sqrt(Q) - r*cx)   (s = -1)
    //
    // The same formula applies for y using cy and fy.
    //
    // For the cylinder we apply this formula to both endpoints (treating each cap
    // as a sphere of radius r), then take the union (min / max) of the four NDC
    // values. This union always contains the cylinder body silhouette for the
    // bond lengths and viewing distances that appear in molecule rendering.

    float x_ndc, y_ndc;

    // X extents — depends on the lateral x and the depth z of each endpoint
    float QxA = eyeA.x*eyeA.x + eyeA.z*eyeA.z - r*r;
    float QxB = eyeB.x*eyeB.x + eyeB.z*eyeB.z - r*r;
    if (QxA > 0.0 && QxB > 0.0) {
        float sqQxA = sqrt(QxA);
        float sqQxB = sqrt(QxB);
        float xA1 = -fx * (eyeA.x*sqQxA - r*eyeA.z) / (eyeA.z*sqQxA + r*eyeA.x);
        float xA2 = -fx * (eyeA.x*sqQxA + r*eyeA.z) / (eyeA.z*sqQxA - r*eyeA.x);
        float xB1 = -fx * (eyeB.x*sqQxB - r*eyeB.z) / (eyeB.z*sqQxB + r*eyeB.x);
        float xB2 = -fx * (eyeB.x*sqQxB + r*eyeB.z) / (eyeB.z*sqQxB - r*eyeB.x);
        float x_left  = min(min(xA1, xA2), min(xB1, xB2));
        float x_right = max(max(xA1, xA2), max(xB1, xB2));
        x_ndc = (aCorner.x < 0.0) ? x_left : x_right;
    } else {
        x_ndc = aCorner.x; // camera inside one cap sphere: cover full screen width
    }

    // Y extents — depends on the lateral y and the depth z of each endpoint
    float QyA = eyeA.y*eyeA.y + eyeA.z*eyeA.z - r*r;
    float QyB = eyeB.y*eyeB.y + eyeB.z*eyeB.z - r*r;
    if (QyA > 0.0 && QyB > 0.0) {
        float sqQyA = sqrt(QyA);
        float sqQyB = sqrt(QyB);
        float yA1 = -fy * (eyeA.y*sqQyA - r*eyeA.z) / (eyeA.z*sqQyA + r*eyeA.y);
        float yA2 = -fy * (eyeA.y*sqQyA + r*eyeA.z) / (eyeA.z*sqQyA - r*eyeA.y);
        float yB1 = -fy * (eyeB.y*sqQyB - r*eyeB.z) / (eyeB.z*sqQyB + r*eyeB.y);
        float yB2 = -fy * (eyeB.y*sqQyB + r*eyeB.z) / (eyeB.z*sqQyB - r*eyeB.y);
        float y_bot = min(min(yA1, yA2), min(yB1, yB2));
        float y_top = max(max(yA1, yA2), max(yB1, yB2));
        y_ndc = (aCorner.y < 0.0) ? y_bot : y_top;
    } else {
        y_ndc = aCorner.y; // camera inside one cap sphere: cover full screen height
    }

    // Use bond midpoint depth for the clip test.
    // The fragment shader will overwrite gl_FragDepth with the actual hit depth.
    vec3  mid     = 0.5 * (eyeA + eyeB);
    vec4  clipMid = uProj * vec4(mid, 1.0);
    float z_ndc   = clipMid.z / clipMid.w;

    gl_Position = vec4(x_ndc, y_ndc, z_ndc, 1.0);

    vPosAEye = eyeA;
    vPosBEye = eyeB;
    vRadius  = aRadius;
    vColor   = aColor;
}
