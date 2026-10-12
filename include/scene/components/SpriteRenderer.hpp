#ifndef SPRITERENDERER_HPP
#define SPRITERENDERER_HPP

#include <filesystem>
#include <memory>

#include "scene/components/Component.hpp"

struct ComponentBuildContext;
struct SDL_Surface;
struct SDL_Texture;

namespace NeuronIDE {
class Component;
}

// Draws an image file into its owner's Transform: x/y is the top-left corner in window pixels,
// width/height the drawn size, and rotation is in degrees clockwise about the rectangle's centre.
//
// The image is decoded on construction, so a missing or unreadable file fails while the scene is
// parsed, before any window opens. It becomes a texture in onRenderTargetReady(), before the
// first frame, and the decoded pixels are released then.
class SpriteRenderer : public Component {
   public:
    // Throws std::invalid_argument for a null owner, an empty path, or an owner without a
    // positive width and height; std::runtime_error when the image cannot be loaded.
    SpriteRenderer(const std::shared_ptr<SceneObject>& owner, std::filesystem::path path);

    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;

    // Uploads the image to `target`. A sprite belongs to one render target: a second call throws
    // std::logic_error.
    void onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) override;

    static std::unique_ptr<Component> createSprite(const NeuronIDE::Component&         protoComp,
                                                   const std::shared_ptr<SceneObject>& owner,
                                                   const ComponentBuildContext&        context);

   private:
    struct SurfaceDeleter {
        void operator()(SDL_Surface* surface) const noexcept;
    };
    struct TextureDeleter {
        // The renderer the texture belongs to, kept alive until the texture is destroyed.
        std::shared_ptr<SDL_Renderer> target;
        void                          operator()(SDL_Texture* texture) const noexcept;
    };

    std::filesystem::path                        imagePath;
    std::unique_ptr<SDL_Surface, SurfaceDeleter> image;
    std::unique_ptr<SDL_Texture, TextureDeleter> texture;
};

#endif  // SPRITERENDERER_HPP
