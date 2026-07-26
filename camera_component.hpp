/**
 * @file camera_component.hpp
 * @brief 相机组件声明（视矩阵 + 投影矩阵）
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "component.hpp"

class TransformComponent;

/**
 * @brief 相机组件 —— 管理视图矩阵和透视投影矩阵
 */
class CameraComponent : public Component {
private:
    float fieldOfView = 45.0f;
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane   = 0.1f;
    float farPlane    = 1000.0f;

    mutable glm::mat4 projectionMatrix = glm::mat4(1.0f);
    mutable bool projectionDirty = true;

public:
    void SetPerspective(float fov, float aspect, float nearPlane_, float farPlane_);

    glm::mat4 GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
};
