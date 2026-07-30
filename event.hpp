#include "entity.hpp"
#include <mutex>
#include <queue>

class Event {
public:
    virtual ~Event() = default;

    virtual const char* GetType() const = 0;
    virtual Event* Clone() const = 0;
};

#define DEFINE_EVENT_TYPE(type) \
    static const char* GetStaticType() { return #type; } \
    const char* GetType() const override { return GetStaticType(); } \
    Event* Clone() const override { return new type(*this); }

class WindowResizeEvent : public Event {
private:
    int width;
    int height;
public:
    WindowResizeEvent(int w, int h) : width(w), height(h) {}

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    DEFINE_EVENT_TYPE(WindowResizeEvent)
};

class KeyPressEvent : public Event {
private:
    int keyCode;
    bool repeat;
public:
    KeyPressEvent(int code, bool isRepeat) : keyCode(code), repeat(isRepeat) {}

    int GetKeyCode() const { return keyCode; }
    bool IsRepeat() const { return repeat; }

    DEFINE_EVENT_TYPE(KeyPressEvent)
};

class CollisionEvent : public Event {
private:
    Entity* entity1;
    Entity* entity2;

public:
    CollisionEvent(Entity* e1, Entity* e2) : entity1(e1), entity2(e2) {}

    Entity* GetEntity1() const { return entity1; }
    Entity* GetEntity2() const { return entity2; }

    DEFINE_EVENT_TYPE(CollisionEvent)
};

class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void OnEvent(Event* event) = 0;
};

class EventDispatcher {
private:
    const Event& event;

public:
    explicit EventDispatcher(const Event& e) : event(e) {}

    template<typename T, typename F>
    bool Dispatch(const F& func) {
        if (event.GetType() == T::GetStaticType()) {
            func(static_cast<const T&>(event));
            return true;
        }
        return false;
    }
};

class EventBus {
private:
    std::vector<EventListener*> listeners;
    std::queue<std::unique_ptr<Event>> eventQueue;
    std::mutex queueMutex;
    bool immediateMode = false;

public:
    void SetImmediateMode(bool mode) {
        immediateMode = mode;
    }

    void AddListener(EventListener* listener) {
        listeners.push_back(listener);
    }

    void RemoveListener(EventListener* listener) {
        std::erase(listeners, listener);
    }

    void PublishEvent(std::unique_ptr<Event> event) {
        if (immediateMode) {
            for (auto listener : listeners) {
                listener->OnEvent(event.get());
            }
        } else {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(std::move(event));
        }
    }

    void ProcessEvents() {
        if (immediateMode) return;
        std::queue<std::unique_ptr<Event>> eventsToProcess;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            std::swap(eventsToProcess, eventQueue);
        }
        while (!eventsToProcess.empty()) {
            auto& event = eventsToProcess.front();
            for (auto listener : listeners) {
                listener->OnEvent(event.get());
            }
            eventsToProcess.pop();
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
    EventBus& GetEventSystem() {
        static EventBus eventSystem;
        return eventSystem;
    }
};