#include "entity.hpp"

class Event {
public:
    virtual ~Event() = default;
};

class CollisionEvent : public Event {
private:
    Entity* entity1;
    Entity* entity2;

public:
    CollisionEvent(Entity* e1, Entity* e2) : entity1(e1), entity2(e2) {}

    Entity* GetEntity1() const { return entity1; }
    Entity* GetEntity2() const { return entity2; }
};

class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void OnEvent(Event* event) = 0;
};

class EventSystem {
private:
    std::vector<EventListener*> listeners;

public:
    void AddListener(EventListener* listener) {
        listeners.push_back(listener);
    }

    void RemoveListener(EventListener* listener) {
        std::erase(listeners, listener);
    }

    void DispatchEvent(Event* event) {
        for (auto listener : listeners) {
            listener->OnEvent(event);
        }
    }
};

class PhysicsComponent : public Component, public EventListener {
public:
    void Initialize() override {
        GetEventSystem().AddListener(this);
    }

    ~PhysicsComponent() override {
        GetEventSystem().RemoveListener(this);
    }

    void OnEvent(Event* event) override {
        if (auto collisionEvent = dynamic_cast<const CollisionEvent*>(event)) {
            // 处理碰撞事件
            Entity* entity1 = collisionEvent->GetEntity1();
            Entity* entity2 = collisionEvent->GetEntity2();
            // 在这里处理碰撞逻辑，例如应用物理反应或触发其他事件
        }
    }

private:
    EventSystem& GetEventSystem() {
        static EventSystem eventSystem;
        return eventSystem;
    }
};