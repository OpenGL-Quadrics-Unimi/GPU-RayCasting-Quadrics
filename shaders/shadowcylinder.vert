#version 410 core

// Per-vertex-divisor = 0
layout(location = 0) in vec2  aCorner;
// Per-instance-divisor = 1
layout(location = 1) in vec3  aPosA;
layout(location = 2) in vec3  aPosB;
layout(location = 3) in float aRadius;
layout(location = 4) in vec3  aColor;   

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform vec3 uShadowRight;  // world-space right vector perpendicular to light dir
uniform vec3 uShadowUp;     // world-space up ""

// Light-space NDC positions of the cylinder endpoints + the cylinder r expressed in NDC. 
//The fragment shader uses these to do a 2D capsule
// signed-distance test for clear cylinder shadow
out vec2  vA_ndc;
out vec2  vB_ndc;
out float vR_ndc;

void main()
{
    // Transform bond endpoints to world space.
    vec3 worldA = (uModel * vec4(aPosA, 1.0)).xyz;
    vec3 worldB = (uModel * vec4(aPosB, 1.0)).xyz;

    // A billboard around the bond midpoint 
    //that fully covers the cylinder silhouette from the light's point of view.
    // Along the axis, the size is bond length + r-Perpendicular to the axis - r
    vec3 mid = 0.5 * (worldA + worldB);

    // Bond axis projected onto the light-perpendicular plane (shadow right/up).
    vec3 axis    = worldB - worldA;
    float halfLen = 0.5 * length(axis) + aRadius;

    // axisRight-component of bond axis along uShadowRight
    float axisR = dot(normalize(axis), uShadowRight);
    float axisU = dot(normalize(axis), uShadowUp);

    // Conservative billboard
    vec3 cornerWorld = mid
                     + aCorner.x * (halfLen * abs(axisR) + aRadius) * uShadowRight
                     + aCorner.y * (halfLen * abs(axisU) + aRadius) * uShadowUp;

    gl_Position = uLightSpaceMatrix * vec4(cornerWorld, 1.0);

    // Project the endpoints and a radius reference point into light NDC.
    vec4 cA = uLightSpaceMatrix * vec4(worldA, 1.0);
    vec4 cB = uLightSpaceMatrix * vec4(worldB, 1.0);
    vA_ndc = cA.xy / cA.w;
    vB_ndc = cB.xy / cB.w;

    vec4 cR = uLightSpaceMatrix * vec4(worldA + aRadius * uShadowRight, 1.0);
    vR_ndc = length(cR.xy / cR.w - vA_ndc);
}
