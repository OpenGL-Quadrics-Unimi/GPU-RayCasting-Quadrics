#version 410 core

in  vec2 TexCoord;
out float FragOcclusion;

// G-buffer inputs
uniform sampler2D gNormal;    // eye-space normals (encoded 0-1)
uniform sampler2D gDepth;     // hardware depth buffer

// SSAO kernel and noise
uniform vec3      uKernel[16];   // hemisphere samples in tangent space
uniform sampler2D uNoiseTex;     // 4×4 random rotation vectors
uniform vec2      uNoiseScale;   // viewport / 4  (to tile the 4×4 texture)

// Camera matrices
uniform mat4 uProj;
uniform mat4 uInvProj;

// Sampling r in eye space
uniform float uRadius;

void main()
{
    float depth = texture(gDepth, TexCoord).r;
    if (depth >= 1.0) { FragOcclusion = 1.0; return; }

    // Reconstruct eye-space position
    vec3 ndc      = vec3(TexCoord, depth) * 2.0 - 1.0;
    vec4 pos4     = uInvProj * vec4(ndc, 1.0);
    vec3 fragPos  = pos4.xyz / pos4.w;

    // Eye-space normal
    vec3 N = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);

    // Random tangent vector from the tiled 4×4 noise texture
    vec3 randomVec = normalize(texture(uNoiseTex, TexCoord * uNoiseScale).xyz * 2.0 - 1.0);

    // TBN to rotate kernel from tangent space into eye space
    vec3 T   = normalize(randomVec - N * dot(randomVec, N));
    vec3 B   = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    float occlusion = 0.0;
    for (int i = 0; i < 16; ++i) {
        // Sample position in eye space
        vec3 samplePos = fragPos + TBN * uKernel[i] * uRadius;

        // Project to get the sample's UV + depth from the G-buffer
        vec4 sampleClip = uProj * vec4(samplePos, 1.0);
        vec2 sampleUV   = (sampleClip.xy / sampleClip.w) * 0.5 + 0.5;

        // Compare sample depth-stored geometry depth
        float sampleDepthNDC = texture(gDepth, sampleUV).r * 2.0 - 1.0;
        vec4  sampleRef4     = uInvProj * vec4(sampleUV * 2.0 - 1.0, sampleDepthNDC, 1.0);
        float sampleDepth    = (sampleRef4.xyz / sampleRef4.w).z;

        // Range check
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (samplePos.z >= sampleDepth + 0.025 ? 0.0 : 1.0) * rangeCheck;
    }

    FragOcclusion = 1.0 - (occlusion / 16.0);
}
