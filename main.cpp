/**
 * @file main.cpp
 * @brief 程序入口
 */

#include "entity.hpp"
#include "transform_component.hpp"
#include "camera_component.hpp"
#include "mesh_component.hpp"
#include "resource.hpp"
#include "shader_resource.hpp"
#include "material.hpp"

int main() {
    // Create resource manager
    ResourceManager resourceManager;

    // Load resources
    auto texture = resourceManager.Load<Texture>("brick");
    auto mesh = resourceManager.Load<Mesh>("cube");
    auto vertexShader = resourceManager.Load<Shader>("basic", vk::ShaderStageFlagBits::eVertex);
    auto fragmentShader = resourceManager.Load<Shader>("basic", vk::ShaderStageFlagBits::eFragment);

    // Use resources
    if (texture && mesh && vertexShader && fragmentShader) {
        // Create material using shaders
        Material material(vertexShader, fragmentShader);

        // Set texture in material
        material.SetTexture("diffuse", texture);

        // Create entity with mesh and material
        Entity entity("MyEntity");
        auto meshComponent = entity.AddComponent<MeshComponent>(mesh.Get(), &material);
    }

    // Resources will be automatically released when handles go out of scope
    // or you can explicitly release them
    resourceManager.Release(texture.GetId());

    return EXIT_SUCCESS;
}
