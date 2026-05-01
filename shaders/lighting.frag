#version 410 core

in  vec2 TexCoord;
out vec4 FragColor;

// G-buffer
uniform sampler2D gDiffuse;   
uniform sampler2D gNormal;    
uniform sampler2D gDepth;    

//reconstruction
uniform mat4 uInvProj;// NDC 

//Light config
uniform vec3  uLightDirEye;   
uniform vec3  uLightColor;
uniform float uAmbient;
uniform float uSpecStrength;
uniform float uShininess;

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

    // Blinn-Phong
    vec3 L = normalize(uLightDirEye);
    vec3 V = normalize(-fragPos);
    vec3 H = normalize(L + V);

    vec3 ambient  = uAmbient * albedo;
    vec3 diffuse  = max(dot(N, L), 0.0) * albedo * uLightColor;
    vec3 specular = pow(max(dot(N, H), 0.0), uShininess)
                    * uSpecStrength * uLightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
