/*
    * Camera.cpp - camera class implementation. 
    Here we define the camera's position, orientation, and projection parameters. 
    The camera is designed to orbit around a target point, allowing for interactive control of the view. 
    The class provides methods to get the view and projection matrices, as well as to manipulate the camera's orientation and distance from the target.
*/


#include "Core/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera(glm::vec3 target, GLfloat distance, GLfloat yaw, GLfloat pitch)
    : Target(target),
      Yaw(yaw),
      Pitch(pitch),
      Distance(distance),
      Fov(ZOOM),
      NearPlane(0.1f),
      FarPlane(500.0f),
      MouseSensitivity(SENSITIVITY)
{}


glm::vec3 Camera::GetPosition() const {
    // Spherical coordinates around Target
    GLfloat cp = std::cos(Pitch);
    return Target + Distance * glm::vec3(
        cp * std::sin(Yaw),
        std::sin(Pitch),
        cp * std::cos(Yaw)
    );
}


glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(GetPosition(), Target, glm::vec3(0.0f, 1.0f, 0.0f));
}


glm::mat4 Camera::GetProjectionMatrix(GLfloat aspect) const {
    return glm::perspective(glm::radians(Fov), aspect, NearPlane, FarPlane);
}

void Camera::ProcessMouseMovement(GLfloat xoffset, GLfloat yoffset,
                                  GLboolean constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    // Prevent flipping past the poles when pitch is out of bounds
    if (constrainPitch) {
        const GLfloat limit = glm::radians(89.0f);
        if (Pitch >  limit) Pitch =  limit;
        if (Pitch < -limit) Pitch = -limit;
    }
}

void Camera::ProcessMouseScroll(GLfloat yoffset) {
    Distance -= yoffset;
    if (Distance < 0.5f) Distance = 0.5f;
}