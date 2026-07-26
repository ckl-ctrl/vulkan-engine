/**
 * @file component.cpp
 * @brief 组件基类默认实现
 */

#include "component.hpp"

Component::~Component() = default;

void Component::Initialize() {}

void Component::Update(float /*deltaTime*/) {}

void Component::Render() {}

void Component::SetOwner(Entity* entity) {
    owner = entity;
}

Entity* Component::GetOwner() const {
    return owner;
}
