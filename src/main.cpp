#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Core/Shader.h"
#include "Core/Renderer.h"
#include "Geometry/Quadric.h" 
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

    // Renderer renderer;

    // Depth test needed so closer atoms draw in front of farther ones
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE); // make the vertex shader control point size

    // Load molecule from PDB file and set camera distance:
    // camera distance is set from the bounding radius
    // so the whole molecule always fits in view
    PDB molecule;
    if (!molecule.load("../data/1crn.pdb"))
        std::cerr << "ERROR: could not load PDB file" << std::endl;
    else
        std::cout << "Loaded " << molecule.Atoms.size() << " atoms." << std::endl;

    camera.Distance = molecule.BoundingRadius * 2.5f;

    // Upload atom positions to the GPU (world space, centred at origin)
    std::vector<glm::vec3> positions;
    positions.reserve(molecule.Atoms.size());
    for (const Atom& a : molecule.Atoms)
        positions.push_back(a.Position);

    GLuint atomVAO, atomVBO;
    glGenVertexArrays(1, &atomVAO);
    glGenBuffers(1, &atomVBO);

    glBindVertexArray(atomVAO);
    glBindBuffer(GL_ARRAY_BUFFER, atomVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 positions.size() * sizeof(glm::vec3),
                 positions.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    Shader atomShader("../shaders/atom.vert", "../shaders/atom.frag");

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLfloat aspect  = (GLfloat)SCR_WIDTH / (GLfloat)SCR_HEIGHT;
        glm::mat4 view  = camera.GetViewMatrix();
        glm::mat4 proj  = camera.GetProjectionMatrix(aspect);
        glm::mat4 model = glm::mat4(1.0f);

        atomShader.use();
        atomShader.setMat4("model", model);
        atomShader.setMat4("view",       view);
        atomShader.setMat4("projection", proj);
        

        glBindVertexArray(atomVAO);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(positions.size()));
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &atomVAO);
    glDeleteBuffers(1, &atomVBO);

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