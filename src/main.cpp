#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Core/Renderer.h"
#include "Core/Shader.h"
#include "Core/Camera.h"
#include "Geometry/PDB.h"
#include "Geometry/Bonds.h"

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f), 5.0f);
GLfloat lastX          = SCR_WIDTH  / 2.0f;
GLfloat lastY          = SCR_HEIGHT / 2.0f;
bool firstMouse        = true;
bool leftButtonPressed = false;

// Actual framebuffer size — differs from SCR_WIDTH/SCR_HEIGHT on Retina/HiDPI
// displays where the framebuffer is 2× the window size in each dimension (this avoids the set of molecules to be put in the bottom left corner of the window and the rest of the window to be empty). Updated in framebuffer_size_callback.
int fbWidth  = SCR_WIDTH;
int fbHeight = SCR_HEIGHT;

glm::vec3 lightDirWorld   = glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f));
glm::vec3 lightColor      = glm::vec3(1.0f, 1.0f, 1.0f);
float     ambientStrength = 0.15f;
float     specStrength    = 0.5f;
float     shininess       = 32.0f;

// Per-instance data for sphere impostors (one entry per atom).
// The 4 billboard corners are shared across all instances via a separate VBO.
struct AtomInstance {
    glm::vec3 Center; // world-space atom centre   (location 1, divisor=1)
    float     Radius; // van der Waals radius       (location 2, divisor=1)
    glm::vec3 Color;  // CPK colour                 (location 3, divisor=1)
};

// Per-instance data for cylinder impostors (one entry per bond).
struct BondInstance {
    glm::vec3 PosA;   // world-space position of atom A  (location 1, divisor=1)
    glm::vec3 PosB;   // world-space position of atom B  (location 2, divisor=1)
    float     Radius; // cylinder radius (Angstroms)     (location 3, divisor=1)
    glm::vec3 Color;  // bond colour                     (location 4, divisor=1)
};

// G-buffer for deferred shading, storing diffuse colour, normal, and depth
// Multiple Render Targets 
struct GBuffer {
    GLuint fbo        = 0;
    GLuint diffuseTex = 0, normalTex = 0, depthTex = 0;
    int    width = 0, height = 0;

    void destroy() {
        if (diffuseTex) glDeleteTextures(1, &diffuseTex);
        if (normalTex)  glDeleteTextures(1, &normalTex);
        if (depthTex)   glDeleteTextures(1, &depthTex);
        if (fbo)        glDeleteFramebuffers(1, &fbo);
        diffuseTex = normalTex = depthTex = fbo = 0;
    }

    void create(int w, int h) {
        destroy(); width = w; height = h;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Diffuse colour — GL_RGB8 is enough (values are in [0,1])
        glGenTextures(1, &diffuseTex);
        glBindTexture(GL_TEXTURE_2D, diffuseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, diffuseTex, 0);

        // Eye-space normals — must be GL_RGB16F because normals are signed floats in [-1,1].
        // GL_RGB8 cannot represent negative values without manual encoding, which would
        // cause visible banding in the lighting pass.
        glGenTextures(1, &normalTex);
        glBindTexture(GL_TEXTURE_2D, normalTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0,
                     GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D, normalTex, 0);

        // Depth;nearest filter — read exact depth values, not interpolated ones.
        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, depthTex, 0);

        // Telling OpenGL we're writing two colour attachments simultaneously.
        GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, bufs);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: G-buffer FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Quadrics", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    

    glfwMakeContextCurrent(window);
    // Set callback function to adjust viewport when window is resized
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Query the actual framebuffer size. On Retina/HiDPI displays this is
    // larger than the window size (e.g. 1600x1200 for an 800x600 window).
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    // G-buffer at full framebuffer resolution.
    GBuffer gbuf;
    gbuf.create(fbWidth, fbHeight);
    // Depth test needed so closer atoms draw in front of farther ones
    glEnable(GL_DEPTH_TEST);

    // Load molecule from PDB file and set camera distance:
    // camera distance is set from the bounding radius
    // so the whole molecule always fits in view
    PDB molecule;
    if (!molecule.load("../data/1crn.pdb"))
        std::cerr << "ERROR: could not load PDB file" << std::endl;
    else
        std::cout << "Loaded " << molecule.Atoms.size() << " atoms." << std::endl;

    Bonds bonds;
    bonds.build(molecule.Atoms);
    std::cout << "Found " << bonds.List.size() << " bonds." << std::endl;

    // PDB::load() already centres atoms at origin and computes BoundingRadius.
    // Set up the orbit camera to frame the whole molecule.
    camera.Target   = glm::vec3(0.0f);
    camera.Distance = molecule.BoundingRadius * 2.5f;
    camera.Yaw   = glm::radians(45.0f);
    camera.Pitch = glm::radians(30.0f);

    // Shared billboard corners — a tiny quad template used by both sphere and
    // cylinder VAOs. Ordered for GL_TRIANGLE_STRIP (no EBO needed).
    static const glm::vec2 quadCorners[4] = {
        {-1.f, -1.f}, { 1.f, -1.f}, {-1.f,  1.f}, { 1.f,  1.f}
    };
    GLuint cornerVBO;
    glGenBuffers(1, &cornerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadCorners), quadCorners, GL_STATIC_DRAW);

    // Sphere impostor — instanced rendering.
    // One AtomInstance per atom; the 4 corners above are reused for every atom.
    // Attribute layout:   loc 0 = Corner (vec2, per-vertex)
    //                     loc 1 = Center (vec3, per-instance)
    //                     loc 2 = Radius (float, per-instance)
    //                     loc 3 = Color  (vec3, per-instance)
    std::vector<AtomInstance> atomInstances;
    atomInstances.reserve(molecule.Atoms.size());
    for (const Atom& a : molecule.Atoms) {
        AtomStyle style = PDB::getAtomStyle(a.Element);
        atomInstances.push_back({a.Position, style.Radius, style.Color});
    }

    GLuint sphereVAO, sphereInstanceVBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereInstanceVBO);

    glBindVertexArray(sphereVAO);
    // loc 0: corner offset — per-vertex (divisor = 0, default)
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);
    // loc 1-3: atom data — per-instance (divisor = 1)
    glBindBuffer(GL_ARRAY_BUFFER, sphereInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 atomInstances.size() * sizeof(AtomInstance),
                 atomInstances.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(AtomInstance), (void*)offsetof(AtomInstance, Center));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(AtomInstance), (void*)offsetof(AtomInstance, Radius));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(AtomInstance), (void*)offsetof(AtomInstance, Color));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    glBindVertexArray(0);

    // -----------------------------------------------------------------------
    // Cylinder impostor — instanced rendering.
    // One BondInstance per bond; same corner template reused.
    // Attribute layout:   loc 0 = Corner (vec2, per-vertex)
    //                     loc 1 = PosA   (vec3, per-instance)
    //                     loc 2 = PosB   (vec3, per-instance)
    //                     loc 3 = Radius (float, per-instance)
    //                     loc 4 = Color  (vec3, per-instance)
    // -----------------------------------------------------------------------
    const float BOND_RADIUS = 0.15f; // Angstroms — visual thickness of all bonds

    std::vector<BondInstance> bondInstances;
    bondInstances.reserve(bonds.List.size());
    for (const Bond& bond : bonds.List) {
        const Atom& a = molecule.Atoms[bond.A];
        const Atom& b = molecule.Atoms[bond.B];
        AtomStyle styleA = PDB::getAtomStyle(a.Element);
        AtomStyle styleB = PDB::getAtomStyle(b.Element);
        bondInstances.push_back({a.Position, b.Position,
                                 BOND_RADIUS,
                                 0.5f * (styleA.Color + styleB.Color)});
    }

    GLuint cylinderVAO, cylinderInstanceVBO;
    glGenVertexArrays(1, &cylinderVAO);
    glGenBuffers(1, &cylinderInstanceVBO);

    glBindVertexArray(cylinderVAO);
    // loc 0: corner offset — per-vertex
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);
    // loc 1-4: bond data — per-instance
    glBindBuffer(GL_ARRAY_BUFFER, cylinderInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 bondInstances.size() * sizeof(BondInstance),
                 bondInstances.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BondInstance), (void*)offsetof(BondInstance, PosA));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(BondInstance), (void*)offsetof(BondInstance, PosB));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BondInstance), (void*)offsetof(BondInstance, Radius));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(BondInstance), (void*)offsetof(BondInstance, Color));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);
    glBindVertexArray(0);

    // -----------------------------------------------------------------------
    // Ground plane — a flat quad rendered below the molecule.
    // It writes into the same G-buffer as spheres and cylinders, so the
    // lighting pass shades it automatically with the same Blinn-Phong model.
    //
    // Y position: one step below the bounding sphere's lowest point, so the
    // ground is always visible no matter which molecule is loaded.
    // Size: 4× the bounding radius — large enough to frame the molecule.
    // -----------------------------------------------------------------------
    float groundY        = -molecule.BoundingRadius - 0.5f;
    float groundHalfSize =  molecule.BoundingRadius * 4.0f;

    // Interleaved layout: position (vec3) | normal (vec3), ordered for GL_TRIANGLE_STRIP.
    float groundVerts[] = {
        -groundHalfSize, groundY, -groundHalfSize,   0.f, 1.f, 0.f,
         groundHalfSize, groundY, -groundHalfSize,   0.f, 1.f, 0.f,
        -groundHalfSize, groundY,  groundHalfSize,   0.f, 1.f, 0.f,
         groundHalfSize, groundY,  groundHalfSize,   0.f, 1.f, 0.f,
    };

    GLuint groundVAO, groundVBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVerts), groundVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    Shader quadricShader("../shaders/quadric.vert", "../shaders/quadric.frag");
    Shader cylinderShader("../shaders/cylinder.vert", "../shaders/cylinder.frag");
    Shader groundShader("../shaders/ground.vert",   "../shaders/ground.frag");

    Shader lightingShader("../shaders/lighting.vert",
                          "../shaders/lighting.frag");

    Renderer screenQuad;
    screenQuad.initQuad();
    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        //I rendered spheres into the G-buffer using ray tracing: the geometry pass is now complete. 
        //The screen will remain black until the lighting pass reads the G-buffer and produces the final output.
        glBindFramebuffer(GL_FRAMEBUFFER, gbuf.fbo);
        glViewport(0, 0, gbuf.width, gbuf.height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLfloat aspect  = (GLfloat)fbWidth / (GLfloat)fbHeight;
        glm::mat4 view  = camera.GetViewMatrix();
        glm::mat4 proj  = camera.GetProjectionMatrix(aspect);
        glm::mat4 model = glm::mat4(1.0f);

        quadricShader.use();
        quadricShader.setMat4("uView",    view);
        quadricShader.setMat4("uProj",    proj);
        quadricShader.setMat4("uModel",   model);
        quadricShader.setMat4("uInvProj", glm::inverse(proj));
        quadricShader.setVec2("uViewport", glm::vec2(fbWidth, fbHeight));

        glBindVertexArray(sphereVAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(atomInstances.size()));
        glBindVertexArray(0);

        cylinderShader.use();
        cylinderShader.setMat4("uView",    view);
        cylinderShader.setMat4("uProj",    proj);
        cylinderShader.setMat4("uModel",   model);
        cylinderShader.setMat4("uInvProj", glm::inverse(proj));
        cylinderShader.setVec2("uViewport", glm::vec2(fbWidth, fbHeight));
        glBindVertexArray(cylinderVAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(bondInstances.size()));
        glBindVertexArray(0);

        // Ground plane: rendered last in the geometry pass so atoms/bonds
        // in front of it correctly occlude it via the depth buffer.
        groundShader.use();
        groundShader.setMat4("uView",  view);
        groundShader.setMat4("uProj",  proj);
        groundShader.setMat4("uModel", model);
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // === LIGHTING PASS ===
       // Read the G-buffer, apply Phong, write to the default framebuffer.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // The fullscreen quad covers everything; depth test would just reject it
        // because the default-FB depth was cleared to 1.0.
        glDisable(GL_DEPTH_TEST);

        lightingShader.use();

        // Bind G-buffer textures to units 0/1/2
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuf.diffuseTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuf.normalTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuf.depthTex);

        lightingShader.setInt("gDiffuse", 0);
        lightingShader.setInt("gNormal",  1);
        lightingShader.setInt("gDepth",   2);

        // Position is reconstructed from depth — needs the same proj as geom pass
        lightingShader.setMat4("uInvProj", glm::inverse(proj));

        // World-space light -> eye space (rotation only for a directional light)
        glm::vec3 lightDirEye = glm::normalize(glm::mat3(view) * lightDirWorld);
        lightingShader.setVec3 ("uLightDirEye",  lightDirEye);
        lightingShader.setVec3 ("uLightColor",   lightColor);
        lightingShader.setFloat("uAmbient",      ambientStrength);
        lightingShader.setFloat("uSpecStrength", specStrength);
        lightingShader.setFloat("uShininess",    shininess);

        screenQuad.drawQuad();

        // Restore depth testing for the next frame's geometry pass
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
}
        

glDeleteVertexArrays(1, &sphereVAO);
glDeleteVertexArrays(1, &cylinderVAO);
glDeleteVertexArrays(1, &groundVAO);
glDeleteBuffers(1, &sphereInstanceVBO);
glDeleteBuffers(1, &cylinderInstanceVBO);
glDeleteBuffers(1, &cornerVBO);
glDeleteBuffers(1, &groundVBO);
gbuf.destroy();
glfwTerminate();
return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    fbWidth  = width;
    fbHeight = height;
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true); 
}

// Move the camera around only while
// the left button is held down. otherwise just tracks the position
// so the next drag starts without a jump.
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    GLfloat xpos = static_cast<GLfloat>(xposIn);
    GLfloat ypos = static_cast<GLfloat>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    GLfloat xoffset = -(xpos - lastX); // negate: drag-right → yaw decreases → molecule rotates right
    GLfloat yoffset =  (ypos - lastY); // drag-down → pitch increases → camera rises → molecule dips
    lastX = xpos;
    lastY = ypos;

    if (leftButtonPressed)
        camera.ProcessMouseMovement(xoffset, yoffset);
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        leftButtonPressed = (action == GLFW_PRESS);
        if (leftButtonPressed) firstMouse = true;
    }
}

// Just zooms in or out
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessMouseScroll(static_cast<GLfloat>(yoffset));
}