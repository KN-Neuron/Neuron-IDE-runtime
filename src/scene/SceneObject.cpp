#include "scene/SceneObject.hpp"
#include <utility>
#include "scene/components/Component.hpp"

SceneObject::SceneObject(std::string n, bool visible) : name(std::move(n)), isVisible(visible) {
    std::cout << "  [SceneObject] Utworzono obiekt: " << name << "\n";
}

void SceneObject::setTransform(Transform t) { transform = t; }

void SceneObject::addComponent(std::unique_ptr<Component> comp) { components.push_back(std::move(comp)); }