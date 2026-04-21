/*Atom data (position + VdW radius)
→ CPU: build 4 vertices per atom with corner offsets
→ Vertex shader: move center to eye space, expand corners along screen plane
→ Rasteriser: fills the quad with fragments
→ Fragment shader: flat CPK colour for now */

#version 410 core
layout(location = 0) in vec3  aCenter; // atom centre in world space
layout(location = 1) in vec2  aCorner; // billboard corner, in [-1, 1]
layout(location = 2) in float aRadius; // van der Waals radius (Angstroms)
layout(location = 3) in vec3  aColor;  // CPK colour

out vec3 vColor;
out vec2 vCorner; 

uniform mat4 view;
uniform mat4 projection;

void main() {
    // 1. Move atom centre from world space to eye space
    vec4 eyeCenter = view * vec4(aCenter, 1.0);

    // 2. Expand the quad in eye space so it covers the sphere's silhouette.
    //    XY in eye space = screen plane, so this keeps the quad facing the camera.
    eyeCenter.xy += aCorner * aRadius;

    // 3. Eye space → clip space
    gl_Position = projection * eyeCenter;

    vColor  = aColor;
    vCorner = aCorner;
}