/**
 * @file scene_culling.hpp
 * @brief 场景视锥体剔除系统
 *
 * 基于相机视锥体，对场景中所有实体进行可见性判断，
 * 剔除视锥体外的对象以减少渲染负担。
 */

#include "camera_component.hpp"

/**
 * @brief 视锥体剔除系统
 *
 * 每帧执行：遍历全部实体 → 获取包围盒 → 变换到世界空间 →
 * 与相机视锥体求交 → 收集可见实体列表。
 */
class CullingSystem {
private:
    Camera* camera;                         ///< 用于获取视锥体的相机
    std::vector<Entity*> visibleEntities;   ///< 当前帧可见实体列表

public:
    /** @brief 构造剔除系统并绑定相机 */
    explicit CullingSystem(Camera* cam) : camera(cam) {}

    /** @brief 更换绑定的相机 */
    void SetCamera(Camera* cam) {
        camera = cam;
    }

    /**
     * @brief 执行视锥体剔除
     *
     * 遍历所有实体，跳过非活跃实体和无 MeshComponent / TransformComponent 的实体。
     * 对每个有效实体：获取包围盒 → 用模型矩阵变换到世界空间 → 与视锥体求交。
     *
     * @param allEntities 场景中所有实体的列表
     */
    void CullScene(const std::vector<Entity*>& allEntities) {
        visibleEntities.clear();

        if (!camera) return;

        // Get camera frustum
        Frustum frustum = camera->GetFrustum();         ///< 获取相机视锥体

        // Check each entity against the frustum
        for (auto entity : allEntities) {
            if (!entity->IsActive()) continue;           ///< 跳过非活跃实体

            auto meshComponent = entity->GetComponent<MeshComponent>();
            if (!meshComponent) continue;                ///< 无网格组件则不可见

            auto transformComponent = entity->GetComponent<TransformComponent>();
            if (!transformComponent) continue;           ///< 无变换组件则无法确定位置

            // Get bounding box of the mesh
            BoundingBox boundingBox = meshComponent->GetBoundingBox();   ///< 模型的 AABB

            // Transform bounding box by entity transform
            boundingBox.Transform(transformComponent->GetTransformMatrix()); ///< AABB → 世界空间 OBB

            // Check if bounding box is visible
            if (frustum.Intersects(boundingBox)) {       ///< 视锥体与包围盒相交测试
                visibleEntities.push_back(entity);       ///< 收集可见实体
            }
        }
    }

    /** @brief 获取当前帧可见实体列表（只读） */
    const std::vector<Entity*>& GetVisibleEntities() const {
        return visibleEntities;
    }
};
