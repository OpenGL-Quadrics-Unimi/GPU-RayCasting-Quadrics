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
#include <random>
#include <cmath>
#include <string>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

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

//Actual framebuffer size differs from SCR_WIDTH/SCR_HEIGHT on Retina/HiDPI

//displays where the framebuffer is 2× the window size in each dimension (this avoids the set of molecules to be put in the bottom left corner of the window and the rest of the window to be empty). Updated in framebuffer_size_callback.
int fbWidth  = SCR_WIDTH;
int fbHeight = SCR_HEIGHT;

glm::vec3 lightDirWorld   = glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f));
glm::vec3 lightColor      = glm::vec3(1.0f, 1.0f, 1.0f);
float     ambientStrength = 0.15f;
float     specStrength    = 0.5f;
float     shininess       = 32.0f;

//Per-instance data for sphere impostors (one entry per atom)
//The 4 billboard corners are shared across all instances via a separate VBO
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

//G-buffer for deferred shading, storing diffuse colour, normal, and depth
//Multiple Render Targets 
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

        //Eye-space normals — must be GL_RGB16F because normals are signed floats in [-1,1]
        //GL_RGB8 cannot represent negative values without manual encoding, which would
        //cause visible banding in the lighting pass.
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

        //Depth;nearest filter — read exact depth values, not interpolated ones
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

        //Telling OpenGL we're writing two colour attachments simultaneously
        GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, bufs);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: G-buffer FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

//Single-channel R32F FBO for the raw SSAO occlusion values and the blur result
struct SSAOBuffer {
    GLuint fbo = 0;
    GLuint tex = 0;

    void destroy() {
        if (tex) glDeleteTextures(1, &tex);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        tex = fbo = 0;
    }

    void create(int w, int h) {
        destroy();
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, w, h, 0,
                     GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: SSAO FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// RGB16F FBO for the lit scene (written by the lighting pass, read by the composite pass).
struct SceneBuffer {
    GLuint fbo = 0, tex = 0;

    void destroy() {
        if (tex) glDeleteTextures(1, &tex);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        tex = fbo = 0;
    }

    void create(int w, int h) {
        destroy();
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: scene FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

//Depth-only FBO for shadow mapping
//Uses a 2048×2048 GL_DEPTH_COMPONENT24 texture; border colour set to 1
//so fragments outside the light frustum are never shadowed
struct ShadowMap {
    GLuint fbo      = 0;
    GLuint depthTex = 0;
    int    size     = 2048;

    void destroy() {
        if (depthTex) glDeleteTextures(1, &depthTex);
        if (fbo)      glDeleteFramebuffers(1, &fbo);
        depthTex = fbo = 0;
    }

    void create() {
        destroy();
        glGenFramebuffers(1, &fbo);

        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                     size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, depthTex, 0);
        //No colour output — explicitly tell OpenGL
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: shadow FBO incomplete\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

int main() {
    //Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    //Create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Quadrics", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    

    glfwMakeContextCurrent(window);
    //Set callback function to adjust viewport when window is resized
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    //Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //Query the actual framebuffer size. On Retina/HiDPI displays this is
    //larger than the window size (e.g. 1600x1200 for an 800x600 window)
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    //ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    //G-buffer at full framebuffer resolution.
    GBuffer gbuf;
    gbuf.create(fbWidth, fbHeight);

    //Shadow map-depth-only-2048*2048
    ShadowMap shadowMap;
    shadowMap.create();

    //SSAO buffers-raw occlusion and blurred result-framebuffer resolution
    SSAOBuffer ssaoBuf, ssaoBlurBuf;
    ssaoBuf.create(fbWidth, fbHeight);
    ssaoBlurBuf.create(fbWidth, fbHeight);

    // Scene buffer — lighting pass renders here; composite pass reads it.
    SceneBuffer sceneBuf;
    sceneBuf.create(fbWidth, fbHeight);

    //Hemisphere kernel-16 samples in tangent space, biased toward the surface
    std::uniform_real_distribution<float> randFloats(0.0f, 1.0f);
    std::default_random_engine rng(42);
    std::vector<glm::vec3> ssaoKernel;
    ssaoKernel.reserve(16);
    for (int i = 0; i < 16; ++i) {
        glm::vec3 sample(randFloats(rng)*2.0f - 1.0f,
                         randFloats(rng)*2.0f - 1.0f,
                         randFloats(rng));
        sample = glm::normalize(sample) * randFloats(rng);
        //interpolation-put more samples close to origin
        float scale = float(i) / 16.0f;
        scale = 0.1f + 0.9f * scale * scale;
        ssaoKernel.push_back(sample * scale);
    }

    //4×4 noise texture-random rotations around the Z axis to break banding
    std::vector<glm::vec3> ssaoNoise;
    ssaoNoise.reserve(16);
    for (int i = 0; i < 16; ++i)
        ssaoNoise.push_back(glm::vec3(randFloats(rng)*2.0f - 1.0f,
                                      randFloats(rng)*2.0f - 1.0f,
                                      0.0f));
    GLuint noiseTex;
    glGenTextures(1, &noiseTex);
    glBindTexture(GL_TEXTURE_2D, noiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0,
                 GL_RGB, GL_FLOAT, ssaoNoise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //Depth test-closer atoms draw in front of farther ones
    glEnable(GL_DEPTH_TEST);

    //Load molecule from PDB file
    //camera distance is set from the bounding r so the whole molecule always fits in view
    PDB molecule;
    if (!molecule.load("../data/1crn.pdb"))
        std::cerr << "ERROR: could not load PDB file" << std::endl;
    else
        std::cout << "Loaded " << molecule.Atoms.size() << " atoms." << std::endl;

    Bonds bonds;
    bonds.build(molecule.Atoms);
    std::cout << "Found " << bonds.List.size() << " bonds." << std::endl;

    //PDB::load()-centres atoms at origin and computes BoundingRadius
    //orbit camera-frame the whole molecule
    camera.Target   = glm::vec3(0.0f);
    camera.Distance = molecule.BoundingRadius * 2.5f;
    camera.Yaw   = glm::radians(45.0f);
    camera.Pitch = glm::radians(30.0f);

    //Shared billboard quad used for both sphere and cylinder
    //Rendered with GL_TRIANGLE_STRIP, so no EBO is needed
    static const glm::vec2 quadCorners[4] = {
        {-1.f, -1.f}, { 1.f, -1.f}, {-1.f,  1.f}, { 1.f,  1.f}
    };
    GLuint cornerVBO;
    glGenBuffers(1, &cornerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadCorners), quadCorners, GL_STATIC_DRAW);

    //Sphere impostor — instanced rendering
    //Each atom has an AtomInstance; the same 4 corners are reused for all atoms
    //Attribute layout:
    //loc 0 = Corner (vec2, per-vertex)
    //loc 1 = Center (vec3, per-instance)
    //loc 2 = Radius (float, per-instance)
    //loc 3 = Color (vec3, per-instance)
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
    //loc 0:corner offset — per-vertex-divisor=0
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);
    //loc 1-3:atom data — per-instance-divisor=1
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


    //Bonds are drawn as cylinders
    //Each bond uses a BondInstance, but corners are reused
    //loc 0=corner, others = position,r,color
    //BOND_RADIUS-thickness
    const float BOND_RADIUS = 0.15f; 

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
    //loc 0: corner offset per-vertex
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);
    //loc 1-4: bond data — per-instance
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

    //Ground plane: flat surface below the molecule
    //Writes to the same G-buffer, so it uses the same lighting as other objects
    //Y-slightly below the bounding sphere-always visible
    //Size-4r so frames the molecule
    float groundY        = -molecule.BoundingRadius - 0.5f;
    float groundHalfSize =  molecule.BoundingRadius * 4.0f;

    //Interleaved layout-position (vec3)|normal (vec3)
    //Ordered for GL_TRIANGLE_STRIP
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

    //Shadow-pass shaders for sphere and cylinder 
    // fragments outside the silhouette are discarded
    Shader shadowSphereShader("../shaders/shadowsphere.vert",   "../shaders/shadowsphere.frag");
    Shader shadowCylShader   ("../shaders/shadowcylinder.vert", "../shaders/shadowcylinder.frag");
    Shader shadowGroundShader("../shaders/shadowground.vert",   "../shaders/shadow.frag");

    //SSAO and blur passes same fullscreen-quad vertex shader
    Shader ssaoShader    ("../shaders/lighting.vert", "../shaders/ssao.frag");
    Shader ssaoBlurShader("../shaders/lighting.vert", "../shaders/ssaoblur.frag");

    Shader lightingShader("../shaders/lighting.vert",
                          "../shaders/lighting.frag");
    Shader compositeShader("../shaders/lighting.vert", "../shaders/composite.frag");
    float exposure    = 1.0f;  // controlled via ImGui
    float ssaoRadius  = 1.0f;  // controlled via ImGui

    Renderer screenQuad;
    screenQuad.initQuad();
    //Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        //Depth map from light's perspective (for shadow map)
        //Used in lighting pass
        //Frustum is set to cover the molecule
        float orthoSize = molecule.BoundingRadius * 1.5f;
        //light source far enough to see the whole scene
        glm::vec3 lightPos = lightDirWorld * (molecule.BoundingRadius * 3.0f);
        glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize,
                                         -orthoSize, orthoSize,
                                          0.1f, molecule.BoundingRadius * 8.0f);
        glm::mat4 lightView = glm::lookAt(lightPos,
                                          glm::vec3(0.0f),
                                          glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProj * lightView;

        //Build two vectors perpendicular to the light direction
        //Used for the shadow billboard plane
        glm::vec3 lightDir  = glm::normalize(lightDirWorld);
        glm::vec3 upGuess   = (glm::abs(glm::dot(lightDir, glm::vec3(0,1,0))) > 0.99f)
                              ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
        glm::vec3 shadowRight = glm::normalize(glm::cross(upGuess, lightDir));
        glm::vec3 shadowUp    = glm::cross(lightDir, shadowRight);

        glm::mat4 model = glm::mat4(1.0f);   // shared identity model for all geometry

        glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
        glViewport(0, 0, shadowMap.size, shadowMap.size);
        glClear(GL_DEPTH_BUFFER_BIT);

        //Spheres
        shadowSphereShader.use();
        shadowSphereShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
        shadowSphereShader.setMat4("uModel",            model);
        shadowSphereShader.setVec3("uShadowRight",      shadowRight);
        shadowSphereShader.setVec3("uShadowUp",         shadowUp);
        glBindVertexArray(sphereVAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(atomInstances.size()));

        //Cylinders
        shadowCylShader.use();
        shadowCylShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
        shadowCylShader.setMat4("uModel",            model);
        shadowCylShader.setVec3("uShadowRight",      shadowRight);
        shadowCylShader.setVec3("uShadowUp",         shadowUp);
        shadowCylShader.setVec2("uShadowMapSize",
                                glm::vec2((float)shadowMap.size, (float)shadowMap.size));
        glBindVertexArray(cylinderVAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(bondInstances.size()));

        //Ground plane
        shadowGroundShader.use();
        shadowGroundShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);
        shadowGroundShader.setMat4("uModel",            model);
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindVertexArray(0);

        //GEOMETRY PASS
        //Spheres rendered into the G-buffer-ray tracing
        //Screen stays black until lighting pass
        glBindFramebuffer(GL_FRAMEBUFFER, gbuf.fbo);
        glViewport(0, 0, gbuf.width, gbuf.height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLfloat aspect  = (GLfloat)fbWidth / (GLfloat)fbHeight;
        glm::mat4 view  = camera.GetViewMatrix();
        glm::mat4 proj  = camera.GetProjectionMatrix(aspect);
        //model is declared above in the shadow pass-identity matrix

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

        //Render bond cylinders
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

        //Ground is rendered last
        //Atoms occlude it via depth buffer
        groundShader.use();
        groundShader.setMat4("uView",  view);
        groundShader.setMat4("uProj",  proj);
        groundShader.setMat4("uModel", model);
        glBindVertexArray(groundVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        //SSAO PASS 
        //Compute per-fragment ambient occlusion into ssaoBuf-R32F
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBuf.fbo);
        glViewport(0, 0, fbWidth, fbHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        ssaoShader.use();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gbuf.normalTex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gbuf.depthTex);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTex);
        ssaoShader.setInt("gNormal",   0);
        ssaoShader.setInt("gDepth",    1);
        ssaoShader.setInt("uNoiseTex", 2);
        ssaoShader.setVec2("uNoiseScale",
                           glm::vec2(float(fbWidth) / 4.0f, float(fbHeight) / 4.0f));
        ssaoShader.setMat4("uProj",    proj);
        ssaoShader.setMat4("uInvProj", glm::inverse(proj));
        ssaoShader.setFloat("uRadius", ssaoRadius);
        for (int i = 0; i < 16; ++i)
            ssaoShader.setVec3("uKernel[" + std::to_string(i) + "]", ssaoKernel[i]);
        screenQuad.drawQuad();
        glEnable(GL_DEPTH_TEST);

        //SSAO BLUR PASS
        //Box-blur the raw occlusion to remove high-frequency noise
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurBuf.fbo);
        glViewport(0, 0, fbWidth, fbHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        ssaoBlurShader.use();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ssaoBuf.tex);
        ssaoBlurShader.setInt("uSSAO", 0);
        screenQuad.drawQuad();
        glEnable(GL_DEPTH_TEST);

        //LIGHTING PASS
        glBindFramebuffer(GL_FRAMEBUFFER, sceneBuf.fbo);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Fullscreen quad covers the whole screen
        // Depth test rejects fragments-depth = 1
        glDisable(GL_DEPTH_TEST);

        lightingShader.use();

        //Bind G-buffer textures to units 0/1/2
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuf.diffuseTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbuf.normalTex);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbuf.depthTex);
        //Shadow map on unit 3
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadowMap.depthTex);
        //Blurred SSAO on unit 4
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, ssaoBlurBuf.tex);

        lightingShader.setInt("gDiffuse",    0);
        lightingShader.setInt("gNormal",     1);
        lightingShader.setInt("gDepth",      2);
        lightingShader.setInt("uShadowMap",  3);
        lightingShader.setInt("uSSAOTex",    4);

        //Position is reconstructed from depth (needs the same proj as geo pass)
        lightingShader.setMat4("uInvProj",          glm::inverse(proj));
        lightingShader.setMat4("uInvView",          glm::inverse(view));
        lightingShader.setMat4("uLightSpaceMatrix", lightSpaceMatrix);

        //Light:world → view space (rotation only)
        glm::vec3 lightDirEye = glm::normalize(glm::mat3(view) * lightDirWorld);
        lightingShader.setVec3 ("uLightDirEye",  lightDirEye);
        lightingShader.setVec3 ("uLightColor",   lightColor);
        lightingShader.setFloat("uAmbient",      ambientStrength);
        lightingShader.setFloat("uSpecStrength", specStrength);
        lightingShader.setFloat("uShininess",    shininess);

        screenQuad.drawQuad();

        //COMPOSITE PASS
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        compositeShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneBuf.tex);
        compositeShader.setInt  ("uScene",    0);
        compositeShader.setFloat("uExposure", exposure);
        screenQuad.drawQuad();

        //Depth test re-enabled
        glEnable(GL_DEPTH_TEST);

        //IMGUI PASS
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Scene Controls");
        ImGui::SliderFloat("Exposure",    &exposure,         0.1f,  3.0f);
        ImGui::SliderFloat("Ambient",     &ambientStrength,  0.0f,  1.0f);
        ImGui::SliderFloat("Specular",    &specStrength,     0.0f,  1.0f);
        ImGui::SliderFloat("Shininess",   &shininess,        1.0f,  128.0f);
        ImGui::SliderFloat("SSAO Radius", &ssaoRadius,       0.1f,  5.0f);
        ImGui::SliderFloat("FOV",         &camera.Fov,       10.0f, 120.0f);
        ImGui::SliderFloat3("Light Dir",  glm::value_ptr(lightDirWorld), -1.0f, 1.0f);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
}
        

ImGui_ImplOpenGL3_Shutdown();
ImGui_ImplGlfw_Shutdown();
ImGui::DestroyContext();

glDeleteVertexArrays(1, &sphereVAO);
glDeleteVertexArrays(1, &cylinderVAO);
glDeleteVertexArrays(1, &groundVAO);
glDeleteBuffers(1, &sphereInstanceVBO);
glDeleteBuffers(1, &cylinderInstanceVBO);
glDeleteBuffers(1, &cornerVBO);
glDeleteBuffers(1, &groundVBO);
gbuf.destroy();
shadowMap.destroy();
ssaoBuf.destroy();
ssaoBlurBuf.destroy();
sceneBuf.destroy();
glDeleteTextures(1, &noiseTex);
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

    if (leftButtonPressed && !ImGui::GetIO().WantCaptureMouse)
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
    if (!ImGui::GetIO().WantCaptureMouse)
        camera.ProcessMouseScroll(static_cast<GLfloat>(yoffset));
}