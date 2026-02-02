cmake_minimum_required(VERSION 3.24)
project(LmaoEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include our cmake modules
include(cmake/Dependencies.cmake)
include(cmake/ShaderCompile.cmake)

# Collect engine sources
file(GLOB_RECURSE ENGINE_SOURCES
    src/core/*.cpp
    src/vulkan/*.cpp
    src/renderer/*.cpp
    src/scene/*.cpp
    src/assets/*.cpp
    src/gui/*.cpp
)

# Engine static library
add_library(lmao_engine STATIC ${ENGINE_SOURCES})
target_include_directories(lmao_engine PUBLIC src)
target_link_libraries(lmao_engine PUBLIC
    volk::volk_headers
    Vulkan::Headers
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    imgui
    tinyobjloader
    stb_image
)
target_compile_definitions(lmao_engine PUBLIC
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL
    VK_NO_PROTOTYPES
    NOMINMAX
)

if(MSVC)
    target_compile_options(lmao_engine PUBLIC /Zc:preprocessor)
    target_compile_options(lmao_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(lmao_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Demo executable
add_executable(lmao_demo src/main.cpp)
target_link_libraries(lmao_demo PRIVATE lmao_engine)

# Compile shaders
compile_shaders(lmao_demo ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders)

# Copy assets and shaders next to executable
add_custom_command(TARGET lmao_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:lmao_demo>/assets
)
add_custom_command(TARGET lmao_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_BINARY_DIR}/shaders $<TARGET_FILE_DIR:lmao_demo>/shaders
)
