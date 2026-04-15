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

Camera::Camera(glm::vec3 target, float distance, float yaw, float pitch)
    : fovY(glm::radians(45.0f))
    , nearZ(0.1f)
    , farZ(500.0f)
    , target(target)
    , yaw(yaw)
    , pitch(pitch)
    , distance(distance)
{}

glm::vec3 Camera::getPosition() const {
    // Spherical coordinates for target
    float cp = std::cos(pitch); //
    return target + distance * glm::vec3(
        cp * std::sin(yaw),
        std::sin(pitch),
        cp * std::cos(yaw)
    );
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(getPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    return glm::perspective(fovY, aspect, nearZ, farZ);
}

void Camera::orbit(float deltaYaw, float deltaPitch) {
    yaw += deltaYaw;
    // Clamp pitch (otheerwise it flips past the poles!!)
    const float limit = glm::radians(89.0f);
    pitch = std::clamp(pitch + deltaPitch, -limit, limit);
}

void Camera::zoom(float deltaDistance) { 
    distance = std::max(0.5f, distance + deltaDistance); // Don't let the camera get too close to the target
}