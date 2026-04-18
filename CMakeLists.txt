cmake_minimum_required(VERSION 3.10)
project(QuadricRaycaster)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External libraries (installed via Homebrew on macOS)
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)

# Build the executable: sources are grouped by module
# (main entry point, Core engine, Geometry, vendored third-party).
add_executable(QuadricRaycaster
    src/main.cpp
    src/Core/Renderer.cpp
    src/Core/Camera.cpp
    src/Geometry/Quadric.cpp
    external/glad/src/glad.c
    src/Geometry/PDB.cpp
)

target_include_directories(QuadricRaycaster PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)

# macOS-friendly OpenGL linking
target_link_libraries(QuadricRaycaster
    OpenGL::GL
    glfw
    glm::glm
)