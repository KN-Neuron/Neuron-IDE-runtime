#ifndef SPRITERENDERER_HPP
#define SPRITERENDERER_HPP

#include <memory>

#include "scene/components/Component.hpp"

class ComponentBuildContext;
class Image;

namespace NeuronIDE {
class Component;
}

// Draws an image into its owner's Transform: x/y is the top-left corner in window pixels,
// width/height the drawn size, and rotation is in degrees clockwise about the rectangle's centre.
//
// The image is decoded while the scene is parsed, so a missing or unreadable file fails before any
// window opens. The scene's ImageLibrary uploads it to the GPU before the first frame. A sprite
// without an image (an empty img_path) draws nothing.
class SpriteRenderer : public Component {
   public:
    // `image` may be null. Throws std::invalid_argument for a null owner and, when there is an
    // image, for an owner whose transform is not finite or has no positive width and height.
    SpriteRenderer(const std::shared_ptr<SceneObject>& owner, std::shared_ptr<const Image> image);

    void update(const Context& context) override;

    // Draws nothing while the image has no texture yet. Throws std::runtime_error when SDL fails
    // to draw it, so a frame without its stimulus is never presented and marked as shown.
    void render(SDL_Renderer* renderer) override;

    static std::unique_ptr<Component> createSprite(const NeuronIDE::Component&         protoComp,
                                                   const std::shared_ptr<SceneObject>& owner,
                                                   const ComponentBuildContext&        context);

   private:
    std::shared_ptr<const Image> image;
};

#endif  // SPRITERENDERER_HPP
