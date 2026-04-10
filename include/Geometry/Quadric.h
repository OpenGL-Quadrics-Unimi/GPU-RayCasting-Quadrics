#pragma once
#include <glm/glm.hpp>

namespace Geometry {
    class Quadric {
        public:
            static glm::mat4 createSphere();
            static glm::mat4 createCylinder();
    };
}