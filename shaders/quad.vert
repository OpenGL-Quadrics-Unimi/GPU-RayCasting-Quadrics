#version 410 core

// Per-vertex (4 corners of a unit quad, values in {0,1})
layout (location = 0) in vec2 aQuadCorner;

// Per-instance (one quad instance = one sphere)
layout (location = 1) in vec3 aCenter;
layout (location = 2) in float aRadius;
layout (location = 3) in vec3 aColor;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;   // animated model rotation (object -> world)

// Paper §3.3, last paragraph: "only the quadric-dependent matrix (M·T)^-1
// needs to be passed from the vertex shader to the fragment shader, since
// the matrix V·P is constant." We go one step further and send the full
// (P·V·M·T)^-1 -- cheap here, cheap there.
out mat4 vInvPMT;   // locations 1..4
out vec3 vCenterEye;
out float vRadius;
out vec3 vColor;

void main()
{
    // Variance matrix T for an isotropic sphere (paper eq. 2):
    //   T = [ r  0  0  cx ]
    //       [ 0  r  0  cy ]
    //       [ 0  0  r  cz ]
    //       [ 0  0  0   1 ]
    // The sphere center is in OBJECT space; uModel rotates the whole
    // molecule into world space.
    mat4 T = mat4(
        aRadius, 0.0,     0.0,     0.0,
        0.0,     aRadius, 0.0,     0.0,
        0.0,     0.0,     aRadius, 0.0,
        aCenter.x, aCenter.y, aCenter.z, 1.0
    );

    mat4 PMT    = uProj * uView * uModel * T;
    mat4 invPMT = inverse(PMT);

    //Bounding box in NDC (paper eq. 5)
    // Rows of PMT are the columns of its transpose.
    mat4 PMTt = transpose(PMT);
    vec4 r1 = PMTt[0];
    vec4 r2 = PMTt[1];
    vec4 r4 = PMTt[3];

    // D = diag(1,1,1,-1) so  a^T D b = a.xyz · b.xyz - a.w·b.w
    float r4Dr4 = dot(r4.xyz, r4.xyz) - r4.w * r4.w;
    float r1Dr4 = dot(r1.xyz, r4.xyz) - r1.w * r4.w;
    float r1Dr1 = dot(r1.xyz, r1.xyz) - r1.w * r1.w;
    float r2Dr4 = dot(r2.xyz, r4.xyz) - r2.w * r4.w;
    float r2Dr2 = dot(r2.xyz, r2.xyz) - r2.w * r2.w;

    float discX = r1Dr4 * r1Dr4 - r4Dr4 * r1Dr1;
    float discY = r2Dr4 * r2Dr4 - r4Dr4 * r2Dr2;

    if (discX < 0.0 || discY < 0.0 || r4Dr4 == 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // degenerate -> cull
        vInvPMT    = mat4(1.0);
        vCenterEye = vec3(0.0);
        vRadius    = 0.0;
        vColor     = vec3(0.0);
        return;
    }

    float sqrtDiscX = sqrt(discX);
    float sqrtDiscY = sqrt(discY);
    float invR4Dr4  = 1.0 / r4Dr4;
    float bx1 = (r1Dr4 - sqrtDiscX) * invR4Dr4;
    float bx2 = (r1Dr4 + sqrtDiscX) * invR4Dr4;
    float by1 = (r2Dr4 - sqrtDiscY) * invR4Dr4;
    float by2 = (r2Dr4 + sqrtDiscY) * invR4Dr4;

    // Frustum cull (paper eq. 5 + an extra screen-test)
    if (bx2 < -1.0 || bx1 > 1.0 || by2 < -1.0 || by1 > 1.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vInvPMT    = mat4(1.0);
        vCenterEye = vec3(0.0);
        vRadius    = 0.0;
        vColor     = vec3(0.0);
        return;
    }

    vec2 ndc = vec2(mix(bx1, bx2, aQuadCorner.x),
                    mix(by1, by2, aQuadCorner.y));

    gl_Position = vec4(ndc, 0.0, 1.0);

    vInvPMT    = invPMT;
    vCenterEye = (uView * uModel * vec4(aCenter, 1.0)).xyz;
    vRadius    = aRadius;
    vColor     = aColor;
}