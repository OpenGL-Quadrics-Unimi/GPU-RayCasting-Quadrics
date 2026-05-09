#version 410 core

// Endpoints in light NDC space and radius, from the vertex shader.
in vec2  vA_ndc;
in vec2  vB_ndc;
in float vR_ndc;

uniform vec2 uShadowMapSize;

float sdCapsule2D(vec2 p, vec2 a, vec2 b, float r)
{
    vec2  pa = p - a;
    vec2  ba = b - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

void main()
{
    vec2 ndc = (gl_FragCoord.xy / uShadowMapSize) * 2.0 - 1.0;
    if (sdCapsule2D(ndc, vA_ndc, vB_ndc, vR_ndc) > 0.0) discard;
}
