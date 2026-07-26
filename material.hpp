/**
 * @file material.hpp
 * @brief 材质数据定义（PBR 参数 + 贴图路径）
 *
 * 基于金属度-粗糙度工作流的 PBR 材质模型，
 * 暂以贴图路径引用资源，后续由渲染器负责加载和绑定 GPU 纹理。
 */

#pragma once

#include <glm/glm.hpp>
#include <string>

/**
 * @brief PBR 材质数据
 *
 * 使用 Disney/glTF 标准的金属度-粗糙度模型。
 * 纹理路径为空字符串时使用默认值（纯色/默认法线等）。
 */
struct Material {
    glm::vec4 baseColor{1.0f};                  ///< RGBA 基础颜色
    float metallic = 0.0f;                      ///< 金属度 [0, 1]，0=非金属，1=金属
    float roughness = 0.5f;                     ///< 粗糙度 [0, 1]，0=光滑，1=粗糙

    std::string baseColorTexture;               ///< 基础颜色贴图路径
    std::string normalTexture;                  ///< 法线贴图路径
    std::string metallicRoughnessTexture;       ///< 金属度-粗糙度合并贴图路径
    std::string occlusionTexture;               ///< 环境光遮蔽贴图路径
    std::string emissiveTexture;                ///< 自发光贴图路径
};
