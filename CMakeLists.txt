cmake_minimum_required(VERSION 3.20)
project(BeachWorldOpenGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

# Moi module co mot cap header/source rieng.
# CMakeLists.txt chi duoc nguoi tich hop quan ly.
add_executable(BeachWorld
    src/main.cpp
    src/terrain.cpp
    src/ocean.cpp
    src/sky.cpp
)

target_include_directories(BeachWorld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(BeachWorld PRIVATE
    glfw
    OpenGL::GL
    GLEW::GLEW
    glm::glm
)

if(MSVC)
    target_compile_options(BeachWorld PRIVATE /W4 /permissive-)
else()
    target_compile_options(BeachWorld PRIVATE -Wall -Wextra -Wpedantic)
endif()
