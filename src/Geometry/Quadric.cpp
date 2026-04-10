#include "Geometry/Quadric.h"
#include <glm/glm.hpp>

namespace Geometry {
    // Basic quadrics for now.
    // Sphere: x^2 + y^2 + z^2 - 1 = 0

    glm::mat4 Quadric::createSphere() {
        glm::mat4 Q(0.0f);
        Q[0][0] = 1.0f;
        Q[1][1] = 1.0f;
        Q[2][2] = 1.0f;
        Q[3][3] = -1.0f;
        return Q;
    }

    glm::mat4 Quadric::createCylinder() {
        glm::mat4 Q(0.0f);
        Q[0][0] = 1.0f;
        Q[1][1] = 1.0f;
        Q[2][2] = 0.0f; 
        Q[3][3] = -1.0f;
        return Q;
    }
}