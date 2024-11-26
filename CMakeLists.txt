cmake_minimum_required(VERSION 3.10.0)

project(OpenGLExample)

set(CMAKE_CXX_STANDARD 17)

### Configure output locations
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT OpenGLExample)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(deps)

file(GLOB_RECURSE SRCS "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

add_executable(OpenGLExample "${SRCS}" "${HEADERS}")
target_include_directories(OpenGLExample PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(OpenGLExample glad glfw glm stb_image assimp)
