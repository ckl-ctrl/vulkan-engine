# 依赖图分析单元测试 —— 纯逻辑测试，不需要 Vulkan 设备

add_executable(GraphTests
    test_dependency_graph.cpp
)

target_include_directories(GraphTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${Vulkan_INCLUDE_DIR}"
)

target_link_libraries(GraphTests PRIVATE
    GTest::gtest_main
)

# MSVC 编译选项（与主项目保持一致）
if(MSVC)
    target_compile_options(GraphTests PRIVATE
        /utf-8
        /permissive-
        /Zc:__cplusplus
        /EHsc
    )
    target_compile_definitions(GraphTests PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    )
endif()
