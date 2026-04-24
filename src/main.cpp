#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Core/Shader.h"
#include "Core/Camera.h"
#include "Geometry/PDB.h"

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

struct ImpostorVertex {
    glm::vec3 Center;
    glm::vec2 Corner;
    float     Radius;
    glm::vec3 Color;
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

        // I used GL_LINEAR on the color targets so that when the G-buffer is sampled at window resolution during the composite pass,
       // the SSAA downsampling looks smoother and cleaner.
        auto mkColor = [&](GLuint& t, GLenum attach) {
            glGenTextures(1, &t);
            glBindTexture(GL_TEXTURE_2D, t);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, attach,
                                   GL_TEXTURE_2D, t, 0);
        };
        mkColor(diffuseTex, GL_COLOR_ATTACHMENT0);
        mkColor(normalTex,  GL_COLOR_ATTACHMENT1);

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
    // G-buffer at full window resolution (supersampling- not yet).
     GBuffer gbuf;
     gbuf.create(SCR_WIDTH, SCR_HEIGHT);
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

    camera.Distance = molecule.BoundingRadius * 2.5f;
        const glm::vec2 corners[4] = {
        {-1.f, -1.f}, { 1.f, -1.f}, { 1.f,  1.f}, {-1.f,  1.f}
    };

    std::vector<ImpostorVertex> vertices;
    std::vector<GLuint>         indices;
    vertices.reserve(molecule.Atoms.size() * 4);
    indices.reserve(molecule.Atoms.size() * 6);

    for (size_t i = 0; i < molecule.Atoms.size(); ++i) {
        const Atom& a     = molecule.Atoms[i];
        AtomStyle   style = PDB::getAtomStyle(a.Element);

        for (int j = 0; j < 4; ++j)
            vertices.push_back({a.Position, corners[j], style.Radius, style.Color});

        GLuint base = static_cast<GLuint>(i * 4);
        indices.insert(indices.end(), {base, base+1, base+2, base, base+2, base+3});
    }

    GLuint impostorVAO, impostorVBO, impostorEBO;
    glGenVertexArrays(1, &impostorVAO);
    glGenBuffers(1, &impostorVBO);
    glGenBuffers(1, &impostorEBO);

    glBindVertexArray(impostorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, impostorVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ImpostorVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impostorEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ImpostorVertex), (void*)offsetof(ImpostorVertex, Center));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImpostorVertex), (void*)offsetof(ImpostorVertex, Corner));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ImpostorVertex), (void*)offsetof(ImpostorVertex, Radius));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(ImpostorVertex), (void*)offsetof(ImpostorVertex, Color));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);

    Shader quadricShader("../shaders/quadric.vert", "../shaders/quadric.frag");

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLfloat aspect  = (GLfloat)SCR_WIDTH / (GLfloat)SCR_HEIGHT;
        glm::mat4 view  = camera.GetViewMatrix();
        glm::mat4 proj  = camera.GetProjectionMatrix(aspect);

        quadricShader.use();
        quadricShader.setMat4("uProj",      proj);
        quadricShader.setMat4("uInvProj",   glm::inverse(proj));
        quadricShader.setMat4("view",       view);
        quadricShader.setMat4("projection", proj);

        glBindVertexArray(impostorVAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &impostorVAO);
    glDeleteBuffers(1, &impostorVBO);
    glDeleteBuffers(1, &impostorEBO);
    gbuf.destroy();
    glfwTerminate();
    return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
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

    GLfloat xoffset =  (xpos - lastX);
    GLfloat yoffset =  (lastY - ypos); // inverted: screen-Y grows downward
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