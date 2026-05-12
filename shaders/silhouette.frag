#version 410 core

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D uScene;

// Applies the Sobel operator to the depth buffer.
// The Sobel kernel measures how fast the depth changes across neighbours.
// A large gradient means a visible geometric edge.
//
// Horizontal kernel (Gx):    Vertical kernel (Gy):
//  -1  0  1                   -1  -2  -1
//  -2  0  2                    0   0   0
//  -1  0  1                    1   2   1
float sobelDepth(vec2 uv, vec2 texel)
{
    float tl = texture(gDepth, uv + vec2(-1,  1) * texel).r;
    float tc = texture(gDepth, uv + vec2( 0,  1) * texel).r;
    float tr = texture(gDepth, uv + vec2( 1,  1) * texel).r;
    float ml = texture(gDepth, uv + vec2(-1,  0) * texel).r;
    float mr = texture(gDepth, uv + vec2( 1,  0) * texel).r;
    float bl = texture(gDepth, uv + vec2(-1, -1) * texel).r;
    float bc = texture(gDepth, uv + vec2( 0, -1) * texel).r;
    float br = texture(gDepth, uv + vec2( 1, -1) * texel).r;

    float gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    float gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

    return sqrt(gx * gx + gy * gy);
}

// Same Sobel operator applied to the normal buffer.
// Detects crease lines: places where the surface normal changes sharply,
// even between two atoms at the same depth.
// The normal is stored in [0, 1] range, so each channel is treated
// as a scalar and the three-channel gradient magnitude is combined at the end.
float sobelNormal(vec2 uv, vec2 texel)
{
    vec3 tl = texture(gNormal, uv + vec2(-1,  1) * texel).rgb;
    vec3 tc = texture(gNormal, uv + vec2( 0,  1) * texel).rgb;
    vec3 tr = texture(gNormal, uv + vec2( 1,  1) * texel).rgb;
    vec3 ml = texture(gNormal, uv + vec2(-1,  0) * texel).rgb;
    vec3 mr = texture(gNormal, uv + vec2( 1,  0) * texel).rgb;
    vec3 bl = texture(gNormal, uv + vec2(-1, -1) * texel).rgb;
    vec3 bc = texture(gNormal, uv + vec2( 0, -1) * texel).rgb;
    vec3 br = texture(gNormal, uv + vec2( 1, -1) * texel).rgb;

    vec3 gx = -tl - 2.0 * ml - bl + tr + 2.0 * mr + br;
    vec3 gy = -tl - 2.0 * tc - tr + bl + 2.0 * bc + br;

    return sqrt(dot(gx, gx) + dot(gy, gy));
}

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(gDepth, 0));

    float edgeDepth  = sobelDepth (TexCoord, texel);
    float edgeNormal = sobelNormal(TexCoord, texel);

    // Scale factors bring each gradient into a roughly [0, 1] range.
    // Depth values are in NDC [0, 1], so their gradients are small numbers.
    // Normal gradients can be larger because the channels span [0, 1] each.
    // These constants can be adjusted to control outline thickness.
    edgeDepth  *= 20.0;
    edgeNormal *= 1.5;

    // Take the stronger of the two signals and clamp to [0, 1].
    float edge = clamp(max(edgeDepth, edgeNormal), 0.0, 1.0);

    vec3 sceneColor = texture(uScene, TexCoord).rgb;

    // Blend the scene colour toward black where an edge is detected.
    FragColor = vec4(mix(sceneColor, vec3(0.0), edge), 1.0);
}
