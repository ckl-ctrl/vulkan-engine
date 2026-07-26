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