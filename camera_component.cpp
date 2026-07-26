/**
 * @file camera_component.cpp
 * @brief 相机组件实现
 */

#include "camera_component.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "entity.hpp"
#include "transform_component.hpp"

void CameraComponent::SetPerspective(float fov, float aspect, float nearPlane_, float farPlane_) {
    fieldOfView = fov;
    aspectRatio = aspect;
    nearPlane   = nearPlane_;
    farPlane    = farPlane_;
    projectionDirty = true;
}

glm::mat4 CameraComponent::GetViewMatrix() const {
    auto* transform = GetOwner()->GetComponent<TransformComponent>();
    if (transform) {
        glm::vec3 position = transform->GetPosition();
        glm::quat rotation = transform->GetRotation();

        glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up      = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        return glm::lookAt(position, position + forward, up);
    }
    return glm::mat4(1.0f);
}

const glm::mat4& CameraComponent::GetProjectionMatrix() const {
    if (projectionDirty) {
        projectionMatrix = glm::perspective(
            glm::radians(fieldOfView),
            aspectRatio,
            nearPlane,
            farPlane
        );
        projectionDirty = false;
    }
    return projectionMatrix;
}
