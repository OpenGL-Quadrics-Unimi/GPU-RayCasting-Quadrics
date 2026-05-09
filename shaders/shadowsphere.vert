#version 410 core

// Per-vertex (divisor = 0)
layout(location = 0) in vec2  aCorner;
// Per-instance (divisor = 1)
layout(location = 1) in vec3  aCenter;
layout(location = 2) in float aRadius;
layout(location = 3) in vec3  aColor;   // unused in shadow pass

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform vec3 uShadowRight;  // world-space right vector perpendicular to light dir
uniform vec3 uShadowUp;     // world-space up vector perpendicular to light dir

out vec2 vCorner;           // billboard corner in [-1, 1]² — used to clip the
                            // billboard to a circle in the fragment shader

void main()
{
    vCorner = aCorner;

    // Build a billboard in world space perpendicular to the light direction.
    // Displacing each corner by ±radius along the two light-perpendicular axes
    // guarantees the billboard always covers the sphere's silhouette from the
    // light's point of view, regardless of its orientation.
    vec3 worldCenter = (uModel * vec4(aCenter, 1.0)).xyz;
    vec3 cornerWorld = worldCenter
                     + aCorner.x * aRadius * uShadowRight
                     + aCorner.y * aRadius * uShadowUp;

    gl_Position = uLightSpaceMatrix * vec4(cornerWorld, 1.0);
}
