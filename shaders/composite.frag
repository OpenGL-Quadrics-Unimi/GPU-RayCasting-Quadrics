#version 410 core

in  vec2 TexCoord;
out vec4 FragColor;

// The fully-lit scene rendered by the lighting pass.
uniform sampler2D uScene;
// Exposure multiplier (brightens or darkens the whole image uniformly)
// 1.0 = no change. 
uniform float uExposure;

void main()
{
    FragColor = vec4(texture(uScene, TexCoord).rgb * uExposure, 1.0);
}
