cmake_minimum_required(VERSION 3.20)

project(GUIEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Vulkan
find_package(Vulkan REQUIRED)

# GLM — 优先使用项目内置的 external/glm
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
    set(GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
else()
    find_package(glm REQUIRED)
endif()

# 源文件
set(SOURCES
    main.cpp
    component.cpp
    entity.cpp
    transform_component.cpp
    mesh_component.cpp
    camera_component.cpp
)

# 头文件（仅用于 IDE 展示，不影响编译）
set(HEADERS
    component.hpp
    concepts.hpp
    entity.hpp
    transform_component.hpp
    mesh.hpp
    material.hpp
    mesh_component.hpp
    camera_component.hpp
)

add_executable(GUIEngine ${SOURCES} ${HEADERS})

target_include_directories(GUIEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GLM_INCLUDE_DIR}"
    "${Vulkan_INCLUDE_DIR}"
)

target_link_libraries(GUIEngine PRIVATE
    Vulkan::Vulkan
)

# MSVC 编译选项
if(MSVC)
    target_compile_options(GUIEngine PRIVATE
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /EHsc
    )
    target_compile_definitions(GUIEngine PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    )
endif()
