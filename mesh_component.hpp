/**
 * @file mesh_component.hpp
 * @brief 网格渲染组件声明
 */

#pragma once

#include <cstdint>
#include <vector>

#include "component.hpp"

struct Mesh;
struct Material;
struct Vertex;

class MeshComponent : public Component {
private:
    Mesh* mesh = nullptr;
    Material* material = nullptr;

public:
    MeshComponent() = default;
    MeshComponent(Mesh* m, Material* mat) : mesh(m), material(mat) {}

    void SetMesh(Mesh* m) { mesh = m; }
    void SetMaterial(Material* mat) { material = mat; }

    Mesh* GetMesh() const { return mesh; }
    Material* GetMaterial() const { return material; }

    void Render() override;
};
