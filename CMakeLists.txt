cmake_minimum_required(VERSION 3.5.0)
project(LightCone VERSION 0.1.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
SET( CMAKE_CXX_FLAGS  "-g -std=c++17 -fPIC -DPIC -fpermissive")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(include/glad)
include_directories(include)

find_package(glfw3 3.3 REQUIRED)

add_executable(${PROJECT_NAME} include/glad/glad.c src/framework.cpp src/MyApp.cpp src/lodepng.cpp)
add_compile_options(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} glfw)