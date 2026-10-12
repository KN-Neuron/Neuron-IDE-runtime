#include "scene/components/SpriteRenderer.hpp"

#include <SDL2/SDL.h>
#include <SDL_image.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentBuildContext.hpp"
#include "scene/components/ComponentRegistry.hpp"

void SpriteRenderer::SurfaceDeleter::operator()(SDL_Surface* surface) const noexcept {
    SDL_FreeSurface(surface);
}

void SpriteRenderer::TextureDeleter::operator()(SDL_Texture* texture) const noexcept {
    SDL_DestroyTexture(texture);
}

SpriteRenderer::SpriteRenderer(const std::shared_ptr<SceneObject>& owner,
                               std::filesystem::path               path)
    : Component(owner), imagePath(std::move(path)) {
    if (!owner) {
        throw std::invalid_argument("SpriteRenderer: owner must not be null");
    }
    const std::string where = "SpriteRenderer on '" + owner->name + "'";

    if (imagePath.empty()) {
        throw std::invalid_argument(where + ": image path is empty");
    }
    // Written so that a NaN size is rejected too.
    const bool hasArea = owner->transform.width > 0.0 && owner->transform.height > 0.0;
    if (!hasArea) {
        throw std::invalid_argument(where + ": the object needs a positive width and height");
    }

    image.reset(IMG_Load(imagePath.string().c_str()));
    if (!image) {
        throw std::runtime_error(where + ": cannot load image '" + imagePath.string() +
                                 "': " + IMG_GetError());
    }
}

void SpriteRenderer::update(const Context& context) { (void)context; }

void SpriteRenderer::render(SDL_Renderer* renderer) {
    const auto ownerPtr = owner.lock();
    if (renderer == nullptr || !texture || !ownerPtr) {
        return;
    }

    const auto&     transform = ownerPtr->transform;
    const SDL_FRect destination{
        static_cast<float>(transform.posX), static_cast<float>(transform.posY),
        static_cast<float>(transform.width), static_cast<float>(transform.height)};

    // A null centre rotates about the centre of `destination`.
    SDL_RenderCopyExF(renderer, texture.get(), nullptr, &destination, transform.rotation, nullptr,
                      SDL_FLIP_NONE);
}

void SpriteRenderer::onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) {
    if (!target) {
        throw std::invalid_argument("SpriteRenderer: render target must not be null");
    }
    if (!image) {
        throw std::logic_error("SpriteRenderer: '" + imagePath.string() +
                               "' is already uploaded to a render target");
    }

    SDL_Texture* created = SDL_CreateTextureFromSurface(target.get(), image.get());
    if (created == nullptr) {
        throw std::runtime_error("SpriteRenderer: cannot create a texture from '" +
                                 imagePath.string() + "': " + SDL_GetError());
    }
    texture = std::unique_ptr<SDL_Texture, TextureDeleter>(created, TextureDeleter{target});
    image.reset();
}

std::unique_ptr<Component> SpriteRenderer::createSprite(const NeuronIDE::Component& protoComp,
                                                        const std::shared_ptr<SceneObject>& owner,
                                                        const ComponentBuildContext& context) {
    return std::make_unique<SpriteRenderer>(owner,
                                            context.resolveAsset(protoComp.renderer().img_path()));
}

REGISTER_COMPONENT(NeuronIDE::Component::kRenderer, SpriteRenderer::createSprite)
