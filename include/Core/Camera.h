#pragma once

#include <glm/glm.hpp>

// OrbitCamera: always looks at a target point, parametrized by
// yaw (rotation around world Y), pitch (tilt up/down) and distance.
// This is the natural control scheme for inspecting a single object
// (like a molecule) from every angle.
class Camera {
public:
    Camera(glm::vec3 target    = glm::vec3(0.0f),
           float     distance  = 20.0f,
           float     yaw       = 0.6f,   // radians
           float     pitch     = 0.3f);  // radians

    // Matrices to send to the shader
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;

    // Camera world-space position 
    glm::vec3 getPosition() const;

    // Interactive controls (will be driven by input in commit #6)
    void orbit(float deltaYaw, float deltaPitch);
    void zoom(float deltaDistance);
    void setTarget(const glm::vec3& t) { target = t; }

    // Projection parameters 
    float fovY;    // vertical field of view, in radians
    float nearZ; // near plane distance
    float farZ; // far plane distance

private:
    glm::vec3 target;
    float yaw;       // radians. Rotation around world Y
    float pitch;     // radians. Tilt "up" or "down" horizon
    float distance;  // from target to eye
};