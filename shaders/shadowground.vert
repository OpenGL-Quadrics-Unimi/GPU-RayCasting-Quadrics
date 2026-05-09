#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;  // unused in shadow pass

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
}
