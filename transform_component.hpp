/**
 * @file transform_component.hpp
 * @brief 实体变换组件声明（位置 / 旋转 / 缩放）
 *
 * 持有局部空间中的位移、四元数旋转和缩放，
 * 按需计算 TRS 模型矩阵并通过缓存避免重复运算。
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "component.hpp"

/**
 * @brief 变换组件 —— 管理实体的位置、旋转、缩放
 *
 * 使用脏标记（dirty flag）延迟计算模型矩阵：
 * 仅在数据变更后首次访问 GetModelMatrix() 时重新计算。
 */
class TransformComponent : public Component {
private:
    glm::vec3 position = glm::vec3(0.0f);               ///< 局部坐标位置
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< 四元数旋转（默认无旋转）
    glm::vec3 scale    = glm::vec3(1.0f);                ///< 各轴缩放比例

    mutable glm::mat4 modelMatrix = glm::mat4(1.0f);     ///< 缓存 TRS = T × R × S
    mutable bool matrixDirty = true;                     ///< 脏标记，为 true 时需重新计算

public:
    /** @brief 设置世界空间位置 */
    void SetPosition(const glm::vec3& pos);

    /** @brief 设置旋转（四元数） */
    void SetRotation(const glm::quat& rot);

    /** @brief 设置各轴缩放 */
    void SetScale(const glm::vec3& s);

    /** @brief 获取当前位置 */
    const glm::vec3& GetPosition() const { return position; }

    /** @brief 获取当前旋转 */
    const glm::quat& GetRotation() const { return rotation; }

    /** @brief 获取当前缩放 */
    const glm::vec3& GetScale() const { return scale; }

    /**
     * @brief 获取模型矩阵（TRS）
     *
     * 若数据已变更，会先重新计算再返回缓存值。
     */
    const glm::mat4& GetModelMatrix() const;
};
