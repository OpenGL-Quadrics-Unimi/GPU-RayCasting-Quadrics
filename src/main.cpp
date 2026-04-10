#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Core/Shader.h"
#include "Core/Renderer.h"
#include "Geometry/Quadric.h" 
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;


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

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    Renderer renderer;

    // Istantiate the buffers for the quad
    renderer.initQuad();

    // Tell OpenGL the size of the rendering window so that it knows
    // how to display the data and coordinates wrt the window.
    // First two parameters: set location of lower left corner of the window
    // Third and fourth parameters: set width and height of the rendering window
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Create the vertex shader and fragment shader, and link them to create a shader program
    Shader myShader("../shaders/quad.vert", "../shaders/quad.frag");

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // input
        processInput(window);
        // rendering commands

        glClearColor(1.0f, 0.3f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        myShader.use();

        // Quadric matrix is defined as a unit sphere for now (for semplicity)
        glm::mat4 Q_matrix = Geometry::Quadric::createSphere();
        int qLoc = glGetUniformLocation(myShader.ID, "Quad");
        glUniformMatrix4fv(qLoc, 1, GL_FALSE, glm::value_ptr(Q_matrix));

        renderer.drawQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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