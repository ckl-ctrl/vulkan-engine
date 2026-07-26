/**
 * @file mesh_component.cpp
 * @brief 网格组件渲染实现
 */

#include "mesh_component.hpp"

#include "entity.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "transform_component.hpp"

void MeshComponent::Render() {
    if (!mesh || !material) return;

    auto* transform = GetOwner()->GetComponent<TransformComponent>();
    if (!transform) return;

    // TODO: 绑定材质和绘制网格（待渲染器接入后实现）
    // material->Bind();
    // material->SetUniform("modelMatrix", transform->GetModelMatrix());
    // mesh->Draw();
}
