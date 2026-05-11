#version 410 core

in vec2  vNDC;
in vec3  vCenterEye;
in float vRadius;
in vec3  vColor;

uniform mat4 uInvProj;   // NDC -> eye space
uniform mat4 uProj;      // eye space -> clip (for depth write)

layout (location = 0) out vec3 oDiffuse;
layout (location = 1) out vec3 oNormal;

void main()
{
    // Reconstruct eye-space ray using the NDC position interpolated from the vertex shader.
    // This avoids any dependency on framebuffer/window size (no uViewport needed).
    vec4 nearEye4 = uInvProj * vec4(vNDC, -1.0, 1.0);
    vec4 farEye4  = uInvProj * vec4(vNDC,  1.0, 1.0);
    vec3 rO = nearEye4.xyz / nearEye4.w;
    vec3 rD = normalize(farEye4.xyz / farEye4.w - rO);

    //Ray-sphere intersection in eye space
    vec3  oc   = rO - vCenterEye;
    float b    = dot(oc, rD);
    float c    = dot(oc, oc) - vRadius * vRadius;
    float disc = b * b - c;
    if (disc < 0.0) discard;             // miss
    float t = -b - sqrt(disc);
    if (t < 0.0) discard;                // sphere is entirely behind the ray origin

    vec3 posEye    = rO + t * rD;
    vec3 normalEye = (posEye - vCenterEye) / vRadius;

    //Depth write (eye -> clip -> NDC -> window)
    vec4 clipPos = uProj * vec4(posEye, 1.0);
    gl_FragDepth = 0.5 * (clipPos.z / clipPos.w) + 0.5;

    //G-buffer writes
    oDiffuse = vColor;
    oNormal  = normalEye * 0.5 + 0.5;
}