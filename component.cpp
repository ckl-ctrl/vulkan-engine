/**
 * @file component.cpp
 * @brief 组件基类默认实现
 */

#include "component.hpp"

Component::~Component() {
    if (state != State::Destroyed) {
        OnDestroy();
        state = State::Destroyed;
    }
}

void Component::Initialize() {
    if (state == State::Uninitialized) {
        state = State::Initialized;
        OnInitialize();
        state = State::Active;
    }
}

void Component::Update(float /*deltaTime*/) {}

void Component::Render() {}

void Component::SetOwner(Entity* entity) {
    owner = entity;
}

Entity* Component::GetOwner() const {
    return owner;
}
