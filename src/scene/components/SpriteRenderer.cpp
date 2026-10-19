#include "scene/components/SpriteRenderer.hpp"

#include <SDL2/SDL.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "Utf8Path.hpp"
#include "neuronide.pb.h"
#include "scene/Image.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentBuildContext.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
// SDL draws in float, and converting a double outside float range is undefined. Also false for
// NaN and infinity.
bool fitsInFloat(double value) {
    return std::abs(value) <= static_cast<double>(std::numeric_limits<float>::max());
}

std::string describe(const SceneObject& owner) { return "SpriteRenderer on '" + owner.name + "'"; }
}  // namespace

SpriteRenderer::SpriteRenderer(const std::shared_ptr<SceneObject>& owner,
                               std::shared_ptr<const Image>        image)
    : Component(owner), image(std::move(image)) {
    if (!owner) {
        throw std::invalid_argument("SpriteRenderer: owner must not be null");
    }
    if (!this->image) {
        return;
    }

    const auto& transform = owner->transform;
    for (const double value :
         {transform.posX, transform.posY, transform.width, transform.height, transform.rotation}) {
        if (!fitsInFloat(value)) {
            throw std::invalid_argument(describe(*owner) +
                                        ": the transform has a non-finite or out-of-range value");
        }
    }
    if (transform.width <= 0.0 || transform.height <= 0.0) {
        throw std::invalid_argument(describe(*owner) +
                                    ": the object needs a positive width and height");
    }
}

void SpriteRenderer::update(const Context& context) { (void)context; }

void SpriteRenderer::render(SDL_Renderer* renderer) {
    const auto ownerPtr = owner.lock();
    if (renderer == nullptr || !image || image->texture() == nullptr || !ownerPtr) {
        return;
    }

    const auto&     transform = ownerPtr->transform;
    const SDL_FRect destination{
        static_cast<float>(transform.posX), static_cast<float>(transform.posY),
        static_cast<float>(transform.width), static_cast<float>(transform.height)};

    // A null centre rotates about the centre of `destination`.
    if (SDL_RenderCopyExF(renderer, image->texture(), nullptr, &destination, transform.rotation,
                          nullptr, SDL_FLIP_NONE) != 0) {
        throw std::runtime_error(describe(*ownerPtr) + ": cannot draw '" +
                                 utf8::toString(image->path()) + "': " + SDL_GetError());
    }
}

std::unique_ptr<Component> SpriteRenderer::createSprite(const NeuronIDE::Component& protoComp,
                                                        const std::shared_ptr<SceneObject>& owner,
                                                        const ComponentBuildContext& context) {
    const std::string& path = protoComp.renderer().img_path();
    return std::make_unique<SpriteRenderer>(
        owner, path.empty() ? nullptr : context.loadImage(utf8::toPath(path)));
}

REGISTER_COMPONENT(NeuronIDE::Component::kRenderer, SpriteRenderer::createSprite)
