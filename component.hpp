/**
 * @file component.hpp
 * @brief ECS 组件基类声明
 *
 * Component 是所有组件的抽象基类，定义了组件的生命周期接口。
 * 通过前置声明 Entity 避免循环依赖，将实体耦合降到最低。
 */

#pragma once

/** @brief 前置声明，Component 仅通过指针引用实体 */
class Entity;

/**
 * @brief 组件基类
 *
 * 所有功能组件必须继承此类。每个组件持有其所属实体的指针，
 * 并实现 Initialize / Update / Render 生命周期方法。
 */
class Component {
protected:
    Entity* owner = nullptr;    ///< 所属实体指针

public:
    virtual ~Component();       ///< 虚析构，保证派生类正确析构

    /** @brief 组件初始化，在 AddComponent 时调用 */
    virtual void Initialize();

    /**
     * @brief 每帧更新
     * @param deltaTime 帧间隔时间（秒）
     */
    virtual void Update(float deltaTime);

    /** @brief 渲染阶段回调 */
    virtual void Render();

    /** @brief 设置所属实体 */
    void SetOwner(Entity* entity);

    /** @brief 获取所属实体 */
    Entity* GetOwner() const;
};
