#include "scene/SceneObject.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "scene/components/Component.hpp"

namespace {
// SDL draws in float, and converting a double outside float range is undefined. Also false for
// NaN and infinity.
bool fitsInFloat(double value) {
    return std::abs(value) <= static_cast<double>(std::numeric_limits<float>::max());
}
}  // namespace

void SceneObject::Transform::requireDrawable(const std::string& who) const {
    for (const double value : {posX, posY, width, height, rotation}) {
        if (!fitsInFloat(value)) {
            throw std::invalid_argument(who +
                                        ": the transform has a non-finite or out-of-range value");
        }
    }
    if (width <= 0.0 || height <= 0.0) {
        throw std::invalid_argument(who + ": the object needs a positive width and height");
    }
}

SceneObject::SceneObject(std::string n, bool active) : name(std::move(n)), isActive(active) {}

void SceneObject::setTransform(const Transform& transform) { this->transform = transform; }

void SceneObject::addComponent(std::unique_ptr<Component> comp) {
    components.push_back(std::move(comp));
}

void SceneObject::update(const Context& ctx) {
    if (!isActive) {
        return;
    }
    for (const auto& comp : components) {
        comp->update(ctx);
    }
}

void SceneObject::render(SDL_Renderer* renderer) {
    if (!isActive) {
        return;
    }
    for (const auto& comp : components) {
        if (!comp->allowsDrawing()) {
            return;
        }
    }
    for (const auto& comp : components) {
        comp->render(renderer);
    }
}

Delegate* SceneObject::getEvent(std::string_view eventName) {
    if (eventName == "activate") {
        return &onActivate;
    }
    if (eventName == "deactivate") {
        return &onDeactivate;
    }
    return nullptr;
}

void SceneObject::dispatchActivateStatusChange(const Context& ctx) {
    if (isActive == wasActive) {
        return;
    }
    wasActive = isActive;
    if (isActive) {
        onActivate.invoke(ctx);
    } else {
        onDeactivate.invoke(ctx);
    }
}
