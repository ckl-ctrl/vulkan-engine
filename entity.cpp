/**
 * @file entity.cpp
 * @brief 实体类非模板方法实现
 */

#include "entity.hpp"

Entity::Entity(const std::string& entityName) :
    name(entityName) {}

const std::string& Entity::GetName() const {
    return name;
}

bool Entity::IsActive() const {
    return active;
}

void Entity::SetActive(bool isActive) {
    active = isActive;
}

void Entity::Initialize() {
    for (auto& component : components) {
        component->Initialize();
    }
}

void Entity::Update(float deltaTime) {
    if (!active) return;
    for (auto& component : components) {
        component->Update(deltaTime);
    }
}

void Entity::Render() {
    if (!active) return;
    for (auto& component : components) {
        component->Render();
    }
}
