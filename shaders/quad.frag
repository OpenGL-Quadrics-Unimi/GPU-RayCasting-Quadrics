#version 410 core
out vec4 FragColor;

in vec2 vNDC;

// Quadric matrix in eye space
uniform mat4 Quad;
// Inverse projection matrix, used to reconstruct the ray direction
uniform mat4 projInverse;

void main() {
    // Ray reconstruction in eye space (from NDC to view ray)

    vec4 viewRay = projInverse * vec4(vNDC, -1.0, 1.0);
    viewRay /= viewRay.w;

    vec4 Ph = vec4(0.0, 0.0, 0.0, 1.0); // ray origin (eye at origin in eye space)
    vec4 Dh = vec4(normalize(viewRay.xyz), 0.0); // ray direction (w=0, no translation)

    //  Ray-quadric intersection (Sigg 2006, eq. 3) 
    // Substituting r(t) = P + tD into the quadric equation p^T Q p = 0 gives:
    //   t²(D^T Q D) + 2t(P^T Q D) + P^T Q P = 0
    float a =       dot(Dh, Quad * Dh);
    float b =       dot(Ph, Quad * Dh);
    float c =       dot(Ph, Quad * Ph);

    float disc = b*b - a*c;
    if (disc < 0.0) discard; // ray missed the quadric

    float t = (-b - sqrt(disc)) / a;
    if (t < 0.0) discard; // intersection behind the camera

    // Surface normal
    // The gradient of p^T Q p is 2Qp, so the normal direction is Qp.
    vec3 hitPos = vec3(Ph) + t * vec3(Dh);
    vec3 normal = normalize((Quad * vec4(hitPos, 1.0)).xyz);

    // Simple diffuse shading 
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    FragColor = vec4(vec3(0.15 + 0.85 * diff), 1.0);
}