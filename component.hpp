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

class ComponentTypeIDSystem {
private:
    static inline size_t nextTypeID = 0;  ///< 下一个可用组件类型 ID
public:
    template <typename T>
    static size_t GetTypeID() {
        static const size_t typeID = nextTypeID++;
        return typeID;
    }
};

/**
 * @brief 组件基类
 *
 * 所有功能组件必须继承此类。每个组件持有其所属实体的指针，
 * 并实现 Initialize / Update / Render 生命周期方法。
 */
class Component {
public:
    enum class State {
        Uninitialized,  ///< 未初始化
        Initialized,    ///< 已初始化
        Active,         ///< 活跃中
        Destroying,     ///< 正在销毁
        Destroyed       ///< 已销毁
    };
protected:
    State state = State::Uninitialized;  ///< 当前组件状态
    Entity* owner = nullptr;    ///< 所属实体指针

public:
    virtual ~Component();       ///< 虚析构，保证派生类正确析构

    template <typename T>
    static size_t GetTypeID() {
        return ComponentTypeIDSystem::GetTypeID<T>();
    }
    
    /** @brief 组件初始化，在 AddComponent 时调用 */
    virtual void Initialize();

    void Destroy() {
        if (state == State::Active) {
            state = State::Destroying;
            OnDestroy();
            state = State::Destroyed;
        }
    }

    bool IsActive() const { return state == State::Active; }

    /** @brief 设置所属实体 */
    void SetOwner(Entity* entity);

    /** @brief 获取所属实体 */
    Entity* GetOwner() const;

protected:
    virtual void OnInitialize() {}  ///< 初始化回调，派生类可重写
    virtual void OnDestroy() {}     ///< 销毁回调，派生类可重写
    virtual void Update(float deltaTime);  ///< 更新回调，派生类可重写
    virtual void Render();      ///< 渲染回调，派生类可重写

    friend class Entity;  ///< 允许 Entity 访问私有成员
};
