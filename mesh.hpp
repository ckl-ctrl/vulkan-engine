/**
 * @file mesh.hpp
 * @brief 网格数据定义（顶点 + 索引）
 *
 * 定义引擎中渲染网格的核心数据结构，
 * 包含顶点属性和索引缓冲区。
 */

#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

/**
 * @brief 顶点数据结构
 *
 * 每个顶点包含位置、法线和纹理坐标，
 * 后续可扩展切线、骨骼权重等属性。
 */
struct Vertex {
    glm::vec3 position;     ///< 局部空间位置
    glm::vec3 normal;       ///< 法线方向
    glm::vec2 texCoord;     ///< 纹理 UV 坐标

    /** @brief 按值比较两个顶点，用于去重 */
    bool operator==(const Vertex& other) const {
        return position == other.position &&
               normal   == other.normal   &&
               texCoord == other.texCoord;
    }
};

/**
 * @brief 网格数据结构
 *
 * 存储顶点和索引的原始数据，
 * 不持有 Vulkan 资源（由渲染器管理 GPU 缓冲）。
 */
struct Mesh {
    std::vector<Vertex> vertices;   ///< 顶点数组
    std::vector<uint32_t> indices;  ///< 索引数组（三角形列表）
};
