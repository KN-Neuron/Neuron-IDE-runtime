#include "scene/SceneObject.hpp"

#include <utility>

#include "scene/components/Component.hpp"

SceneObject::SceneObject(std::string n, bool visible) : name(std::move(n)), isVisible(visible) {
    std::cout << "  [SceneObject] Utworzono obiekt: " << name << "\n";
}

void SceneObject::setTransform(const Transform& transform) { this->transform = transform; }

void SceneObject::addComponent(std::unique_ptr<Component> comp) {
    components.push_back(std::move(comp));
}

void SceneObject::update(const Context& ctx) {
    for (const auto& comp : components) {
        comp->update(ctx);
    }
}

void SceneObject::render(SDL_Renderer* renderer) {
    for (const auto& comp : components) {
        comp->render(renderer);
    }
}