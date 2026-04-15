#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Default camera values
const GLfloat YAW         =  0.6f;
const GLfloat PITCH       =  0.3f;
const GLfloat DISTANCE    = 20.0f;
const GLfloat SENSITIVITY =  0.005f;
const GLfloat ZOOM        = 45.0f;


// OrbitCamera: always looks at a target point, parametrized by
// yaw (rotation around world Y), pitch (tilt up/down) and distance.

class Camera {
public:
    glm::vec3 Target;
    GLfloat   Yaw;
    GLfloat   Pitch;
    GLfloat   Distance;

    // Projection parameters
    GLfloat Fov;         // vertical FOV, in degrees
    GLfloat NearPlane;
    GLfloat FarPlane;

    GLfloat MouseSensitivity;

    Camera(glm::vec3 target   = glm::vec3(0.0f),
           GLfloat   distance = DISTANCE,
           GLfloat   yaw      = YAW,
           GLfloat   pitch    = PITCH);

    // Returns the view matrix, built with glm::lookAt
    glm::mat4 GetViewMatrix() const;

    // Returns the projection matrix for the given aspect ratio
    glm::mat4 GetProjectionMatrix(GLfloat aspect) const;

    // Returns the camera position in world space
    glm::vec3 GetPosition() const;

   // Reads a mouse movement and rotates the camera around the target
    void ProcessMouseMovement(GLfloat xoffset, GLfloat yoffset,
                              GLboolean constrainPitch = GL_TRUE);

    // Reads a mouse scroll and moves the camera closer or farther
    void ProcessMouseScroll(GLfloat yoffset);

};