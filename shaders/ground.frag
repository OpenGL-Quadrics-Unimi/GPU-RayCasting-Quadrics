#version 410 core

in vec3 vNormalEye;
in vec3 vColor;

// writes to G-buffer (oDiffuse at loc 0, oNormal at loc 1), same format as sphere/cylinder
layout(location = 0) out vec3 oDiffuse;
layout(location = 1) out vec3 oNormal;

void main()
{
    oDiffuse = vColor;
    oNormal  = normalize(vNormalEye) * 0.5 + 0.5;
}
