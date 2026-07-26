/**
 * @file transform_component.cpp
 * @brief 变换组件实现
 */

#include "transform_component.hpp"

#include <glm/gtc/matrix_transform.hpp>

void TransformComponent::SetPosition(const glm::vec3& pos) {
    position = pos;
    matrixDirty = true;
}

void TransformComponent::SetRotation(const glm::quat& rot) {
    rotation = rot;
    matrixDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& s) {
    scale = s;
    matrixDirty = true;
}

const glm::mat4& TransformComponent::GetModelMatrix() const {
    if (matrixDirty) {
        // TRS = Translation × Rotation × Scale
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMatrix    = glm::mat4_cast(rotation);
        glm::mat4 scaleMatrix       = glm::scale(glm::mat4(1.0f), scale);

        modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;
        matrixDirty = false;
    }
    return modelMatrix;
}
