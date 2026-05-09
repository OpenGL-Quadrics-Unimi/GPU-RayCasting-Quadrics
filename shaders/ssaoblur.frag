#version 410 core

in  vec2 TexCoord;
out float FragOcclusion;

uniform sampler2D uSSAO;  // raw SSAO result (R32F)

// 4×4 box blur: average the 16 nearest texels to smooth the noisy SSAO signal.
void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
    float result = 0.0;
    for (int x = -2; x <= 1; ++x) {
        for (int y = -2; y <= 1; ++y) {
            result += texture(uSSAO, TexCoord + vec2(x, y) * texelSize).r;
        }
    }
    FragOcclusion = result / 16.0;
}
