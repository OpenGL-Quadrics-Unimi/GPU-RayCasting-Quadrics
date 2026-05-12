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

// Shadow map 
uniform sampler2D uShadowMap;
uniform mat4      uLightSpaceMatrix;

// Blurred SSAO occlusion 
uniform sampler2D uSSAOTex;

// PCF: 16-sample Poisson-disk filter over the shadow map.
// Returns a value in [0,1]; 0 = fully in shadow, 1 = fully lit.
// A small bias prevents shadow acne caused by depth-map precision limits.
float shadowFactor(vec4 lightSpacePos)
{
    // Perspective divide → NDC, then remap to [0,1] for texture lookup.
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;

    // Outside the light frustum: treat as lit (border colour is white = 1.0).
    if (proj.z > 1.0) return 1.0;

    float currentDepth = proj.z;
    float bias         = 0.005;

    // 16-sample Poisson-disk PCF (offsets spread in a circle so the
    // soft penumbra edge is round rather than the square you get from
    // a regular 3×3 grid kernel)
    const vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
        vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
        vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
        vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
        vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
        vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
    );

    vec2  texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow    = 0.0;
    for (int i = 0; i < 16; ++i) {
        float pcfDepth = texture(uShadowMap,
                                 proj.xy + poissonDisk[i] * texelSize * 1.5).r;
        shadow += (currentDepth - bias > pcfDepth) ? 0.0 : 1.0;
    }
    return shadow / 16.0;
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
