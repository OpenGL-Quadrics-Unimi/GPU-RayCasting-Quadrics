#version 410 core

in vec3  vCenterEye;
in float vRadius;
in vec3  vColor;
in vec2  vNDC;       // interpolated NDC position 

uniform mat4 uInvProj;   // eye space ray reconstruction
uniform mat4 uProj;      // eye space → clip 

out vec4 FragColor;

void main()
{
    // Reconstruct eye-space ray from the interpolated NDC position.
    // Using vNDC for ray reconstruction is more accurate than using gl_FragCoord, which is in window space and has less precision.
    
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

    // Simple diffuse shading — temporary until G-buffer is set up in commit #15
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff    = max(dot(normalEye, lightDir), 0.15);
    FragColor     = vec4(vColor * diff, 1.0);
}