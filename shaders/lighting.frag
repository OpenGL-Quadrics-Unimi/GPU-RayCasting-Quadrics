#version 410 core

in  vec2 TexCoord;
out vec4 FragColor;

// G-buffer
uniform sampler2D gDiffuse;
uniform sampler2D gNormal;
uniform sampler2D gDepth;

//reconstruction
uniform mat4 uInvProj;// NDC
uniform mat4 uInvView; // eye space -> world space

//Light config
uniform vec3  uLightDirEye;
uniform vec3  uLightColor;
uniform float uAmbient;
uniform float uSpecStrength;
uniform float uShininess;

// Shadow map (sampler2DShadow: hardware bilinear depth comparison).
uniform sampler2DShadow uShadowMap;
uniform mat4            uLightSpaceMatrix;
uniform bool            uShadowsEnabled;

// Blurred SSAO occlusion 
uniform sampler2D uSSAOTex;

// 64-sample adaptive PCF, following Sigg et al. (Section 4, page 5).
// Returns a value in [0,1]: 0 = fully in shadow, 1 = fully lit.
float shadowFactor(vec4 lightSpacePos)
{
    if (!uShadowsEnabled) return 1.0;

    // Perspective divide, then remap NDC [-1,1] to texture range [0,1].
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;

    // Outside the light frustum: border colour is 1.0 -> always lit.
    if (proj.z > 1.0) return 1.0;

    // Reference depth for the hardware compare; bias hides acne.
    float refDepth  = proj.z - 0.005;
    vec2  texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));

    // Samples are placed on a Vogel disk: every successive sample is rotated
    // by the golden angle, giving an even circular spread without clumping.
    // Each texture() call is a hardware-filtered depth compare because the
    // shadow map is configured with GL_LINEAR + GL_COMPARE_REF_TO_TEXTURE.
    //
    // Adaptive strategy: take 8 samples first. If they all agree (every
    // sample either fully lit or fully shadowed) the fragment is outside the
    // penumbra and the remaining 56 lookups can be skipped.
    const float GOLDEN_ANGLE = 2.39996323;
    const float radius       = 1.5;

    float shadow = 0.0;
    for (int i = 0; i < 8; ++i) {
        float r     = sqrt((float(i) + 0.5) / 64.0);
        float theta = float(i) * GOLDEN_ANGLE;
        vec2  off   = r * vec2(cos(theta), sin(theta));
        shadow += texture(uShadowMap,
                          vec3(proj.xy + off * texelSize * radius, refDepth));
    }

    if (shadow == 0.0 || shadow == 8.0)
        return shadow / 8.0;

    // Inside the penumbra: take the remaining 56 samples.
    for (int i = 8; i < 64; ++i) {
        float r     = sqrt((float(i) + 0.5) / 64.0);
        float theta = float(i) * GOLDEN_ANGLE;
        vec2  off   = r * vec2(cos(theta), sin(theta));
        shadow += texture(uShadowMap,
                          vec3(proj.xy + off * texelSize * radius, refDepth));
    }
    return shadow / 64.0;
}

void main()
{
    float depth = texture(gDepth, TexCoord).r;

    // Background: we already set the depth to 1.0 in the geometry pass
    // Don’t draw these so the background color is visible
    if (depth >= 1.0) discard;

    //view space
    vec3 ndc       = vec3(TexCoord, depth) * 2.0 - 1.0;
    vec4 viewPos4  = uInvProj * vec4(ndc, 1.0);
    vec3 fragPos   = viewPos4.xyz / viewPos4.w;

    //decoded the normal (it was previously compressed 0–1 range)
    vec3 N = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);

    vec3 albedo = texture(gDiffuse, TexCoord).rgb;

    // Reconstruct world-space position for shadow map lookup.
    vec4 worldPos4    = uInvView * vec4(fragPos, 1.0);
    vec4 lightSpacePos = uLightSpaceMatrix * worldPos4;
    float shadow      = shadowFactor(lightSpacePos);

    // Blinn-Phong
    vec3 L = normalize(uLightDirEye);
    vec3 V = normalize(-fragPos);
    vec3 H = normalize(L + V);

    float ao      = max(texture(uSSAOTex, TexCoord).r, 0.4);
    vec3 ambient  = uAmbient * albedo * ao;
    vec3 diffuse  = shadow * max(dot(N, L), 0.0) * albedo * uLightColor;
    vec3 specular = shadow * pow(max(dot(N, H), 0.0), uShininess)
                    * uSpecStrength * uLightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
