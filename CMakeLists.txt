cmake_minimum_required(VERSION 3.10)
project(QuadricRaycaster)

# Set standard C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to your local headers
include_directories(include)

# Find Homebrew packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Define source files
set(SOURCES 
    src/main.cpp 
    src/Renderer.cpp
    src/glad.c
)

# Create the executable
add_executable(QuadricRaycaster 
    src/main.cpp 
    src/Core/Renderer.cpp 
    src/Geometry/Quadric.cpp
    # Add glad.c here if it's compiled with your source
)

# Explicitly define the include directory for the target
target_include_directories(QuadricRaycaster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries (macOS specific OpenGL linking)
target_link_libraries(QuadricRaycaster
    OpenGL::GL
    glfw
    glm::glm
)