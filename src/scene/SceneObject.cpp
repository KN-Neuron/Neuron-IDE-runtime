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

SceneObject::SceneObject(std::string n, bool visible) : name(std::move(n)), isVisible(visible) {}

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