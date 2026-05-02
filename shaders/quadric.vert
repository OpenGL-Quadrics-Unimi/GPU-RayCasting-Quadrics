/* Atom impostor pipeline (Sigg et al. 2006):
   CPU     → 1 AtomInstance per atom + shared 4-corner quad template
   Vertex  → move centre to eye space, expand corners in screen plane
   Raster  → fills the quad with fragments
   Fragment→ ray-sphere intersection, depth write, G-buffer output

   Instanced rendering: the 4 quad corners (location 0) are per-vertex;
   atom data (locations 1-3) are per-instance (glVertexAttribDivisor = 1). */

#version 410 core

// Per-vertex (divisor = 0): one of the 4 billboard corners in [-1, 1]²
layout(location = 0) in vec2  aCorner;

// Per-instance (divisor = 1): one atom
layout(location = 1) in vec3  aCenter; // atom centre in world space
layout(location = 2) in float aRadius; // van der Waals radius (Angstroms)
layout(location = 3) in vec3  aColor;  // CPK colour

out vec3  vCenterEye; // sphere centre in eye space (for ray-sphere intersection)
out float vRadius;    // van der Waals radius, passed to fragment shader
out vec3  vColor;     // CPK colour
out vec2  vNDC;       // NDC position of this corner — interpolated to give per-fragment ray direction

uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;

void main() {
    // 1. Move atom centre from world space to eye space
    vec4 eyeCenter = uView * uModel * vec4(aCenter, 1.0);

    // 2. Pass centre and radius before expanding — fragment shader needs
    //    the original sphere centre, not the expanded corner position
    vCenterEye = eyeCenter.xyz;
    vRadius    = aRadius;
    vColor     = aColor;

    // 3. Expand the quad in eye space so it covers the sphere's silhouette.
    //    XY in eye space is the screen plane, so this keeps the quad facing the camera.
    eyeCenter.xy += aCorner * aRadius;

    // 4. Eye space → clip space
    gl_Position = uProj * eyeCenter;

    // 5. Store NDC position of this corner.
    //    All four corners share the same clip-space w (same eye-space z), so this
    //    varying interpolates linearly across the quad — giving the correct NDC at
    //    every fragment without needing the viewport dimensions.
    vNDC = gl_Position.xy / gl_Position.w;
}
