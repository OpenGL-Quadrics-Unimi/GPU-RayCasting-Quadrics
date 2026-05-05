#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;

out vec3 vNormalEye;
out vec3 vColor;

// transforms vertices, outputs eye-space normal and grey colour
void main()
{
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);

    // Transform normal to eye space.
    // The ground plane has no non-uniform scaling, so the normal matrix
    // simplifies to mat3(uView * uModel).
    vNormalEye = normalize(mat3(uView * uModel) * aNormal);

    // Neutral grey color for the ground plane; lighting is done in world space so it doesn't matter if we transform it here.
    vColor = vec3(0.50, 0.50, 0.50);
}
