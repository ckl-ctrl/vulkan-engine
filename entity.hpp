/**
 * @file entity.hpp
 * @brief ECS 实体类
 *
 * Entity 是组件的容器，管理组件的增删查及生命周期调度。
 * 模板方法（AddComponent / GetComponent / RemoveComponent）定义在头文件中，
 * 非模板方法（Initialize / Update / Render）实现在 entity.cpp。
 */

#pragma once

#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "component.hpp"
#include "concepts.hpp"

/**
 * @brief 实体类 —— 组件的容器
 *
 * 实体本身不包含行为逻辑，所有功能由附着的 Component 对象提供。
 * 使用 unique_ptr 管理组件生命周期，
 * 提供模板方法按类型增删查组件。
 */
class Entity {
public:
    /** @brief 组件指针别名 */
    using ComponentPtr = std::unique_ptr<Component>;

private:
    std::string name;                     ///< 实体名称
    bool active = true;                   ///< 激活状态，非激活时跳过 Update/Render
    std::vector<ComponentPtr> components; ///< 持有所有组件

public:
    /** @brief 构造实体并命名 */
    explicit Entity(const std::string& entityName);

    /** @brief 虚析构，保证派生类正确析构 */
    virtual ~Entity() = default;

    /** @brief 获取实体名称 */
    const std::string& GetName() const;

    /** @brief 查询实体是否激活 */
    bool IsActive() const;

    /** @brief 设置激活状态 */
    void SetActive(bool isActive);

    /** @brief 初始化所有组件 */
    void Initialize();

    /**
     * @brief 更新所有组件
     * @param deltaTime 帧间隔时间（秒）
     */
    void Update(float deltaTime);

    /** @brief 渲染所有组件 */
    void Render();

    /**
     * @brief 添加一个组件到此实体
     *
     * @tparam T   组件类型，必须继承自 Component
     * @tparam Args 组件构造参数类型
     * @param args  转发给 T 构造函数的参数
     * @return      指向新创建组件的裸指针（不转移所有权）
     */
    template<typename T, typename... Args>
        requires is_base_of<Component, T>
    T* AddComponent(Args&&... args) {
        auto& component = components.emplace_back(
            std::make_unique<T>(std::forward<Args>(args)...)
        );
        component->SetOwner(this);
        component->Initialize();
        return static_cast<T*>(component.get());
    }

    /**
     * @brief 查找指定类型的第一个组件
     *
     * @tparam T 要查找的组件类型
     * @return   指向组件的裸指针，未找到返回 nullptr
     */
    template<typename T>
    T* GetComponent() {
        auto view = components
            | std::views::filter([](auto& uptr) { return dynamic_cast<T*>(uptr.get()); })
            | std::views::transform([](auto& uptr) { return static_cast<T*>(uptr.get()); });
        auto it = view.begin();
        return it != view.end() ? *it : nullptr;
    }

    /**
     * @brief 移除所有指定类型的组件
     *
     * @tparam T 要移除的组件类型
     * @return   true 表示至少移除了一个组件
     */
    template<typename T>
    bool RemoveComponent() {
        auto oldSize = components.size();
        std::erase_if(components, [](auto& uptr) { return dynamic_cast<T*>(uptr.get()) != nullptr; });
        return components.size() != oldSize;
    }
};
