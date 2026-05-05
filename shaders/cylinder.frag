#version 410 core

in vec3  vPosAEye;
in vec3  vPosBEye;
in float vRadius;
in vec3  vColor;

uniform mat4 uInvProj;
uniform mat4 uProj;
uniform vec2 uViewport;

layout(location = 0) out vec3 oDiffuse;
layout(location = 1) out vec3 oNormal;

void main()
{
    // Reconstruct eye-space ray from this fragment's screen position
    vec2 ndc      = (gl_FragCoord.xy / uViewport) * 2.0 - 1.0;
    vec4 nearEye4 = uInvProj * vec4(ndc, -1.0, 1.0);
    vec4 farEye4  = uInvProj * vec4(ndc,  1.0, 1.0);
    vec3 rO = nearEye4.xyz / nearEye4.w;
    vec3 rD = normalize(farEye4.xyz / farEye4.w - rO);

    // Ray-cylinder intersection (infinite cylinder along axis A→B)
    vec3  d   = normalize(vPosBEye - vPosAEye);
    float len = length(vPosBEye - vPosAEye);

    vec3  Q  = rO - vPosAEye;
    vec3  qc = Q  - dot(Q,  d) * d;   // component of Q perpendicular to axis
    vec3  rc = rD - dot(rD, d) * d;   // component of rD perpendicular to axis

    float a    = dot(rc, rc);
    float b    = dot(qc, rc);
    float c    = dot(qc, qc) - vRadius * vRadius;
    float disc = b * b - a * c;

    if (disc < 0.0 || a < 1e-6) discard;

    float sqrtDisc = sqrt(disc);
    float t1 = (-b - sqrtDisc) / a;
    float t2 = (-b + sqrtDisc) / a;
    float t  = (t1 > 0.0) ? t1 : t2;
    if (t < 0.0) discard;

    // Discard hits outside the end caps
    vec3  hit  = rO + t * rD;
    float proj = dot(hit - vPosAEye, d);
    if (proj < 0.0 || proj > len) discard;

    vec3 axisHit   = vPosAEye + proj * d;
    vec3 normalEye = (hit - axisHit) / vRadius;

    // Write correct depth
    vec4 clipPos = uProj * vec4(hit, 1.0);
    gl_FragDepth = 0.5 * (clipPos.z / clipPos.w) + 0.5;

    oDiffuse = vColor;
    oNormal  = normalEye * 0.5 + 0.5;
}
