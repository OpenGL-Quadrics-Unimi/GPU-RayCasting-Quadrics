cmake_minimum_required(VERSION 3.10)
project(QuadricRaycaster)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External libraries (installed via Homebrew on macOS)
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)

# ImGui sources (vendored in external/imgui/)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Build the executable: sources are grouped by module
# (main entry point, Core engine, Geometry, vendored third-party).
add_executable(QuadricRaycaster
    src/main.cpp
    src/Core/Renderer.cpp
    src/Core/Camera.cpp
    src/Geometry/Quadric.cpp
    external/glad/src/glad.c
    src/Geometry/PDB.cpp
    src/Geometry/Bonds.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(QuadricRaycaster PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# macOS-friendly OpenGL linking
target_link_libraries(QuadricRaycaster
    OpenGL::GL
    glfw
    glm::glm
)