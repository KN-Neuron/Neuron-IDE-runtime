#ifndef BITMAP_HPP
#define BITMAP_HPP

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Surface;
struct SDL_Texture;
class SDL_Renderer;

// Pixels in memory, plus their texture once uploaded to a render target. The pixels are kept after
// the upload so the texture can be rebuilt when the render target loses it
// (SDL_RENDER_DEVICE_RESET).
class Bitmap {
   public:
    // How the texture is sampled when it is drawn rotated or at another size.
    enum class ScaleMode : std::uint8_t { Nearest, Linear };

    // Takes ownership of `pixels`, which must not be null. `description` says what they are in
    // error messages.
    Bitmap(SDL_Surface* pixels, std::string description, ScaleMode scaleMode = ScaleMode::Nearest);

    // Creates the texture on `target`, replacing any earlier one. Throws std::invalid_argument for
    // a null target and std::runtime_error when SDL cannot create the texture, e.g. when the bitmap
    // is larger than the GPU's maximum texture size.
    void upload(const std::shared_ptr<SDL_Renderer>& target);

    // Null until upload() has succeeded.
    [[nodiscard]] SDL_Texture* texture() const noexcept { return gpuTexture.get(); }

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;

   private:
    struct SurfaceDeleter {
        void operator()(SDL_Surface* surface) const noexcept;
    };
    struct TextureDeleter {
        // SDL frees a renderer's textures along with it, so destroying a texture after its
        // renderer would be a use-after-free. The texture keeps its renderer alive instead.
        std::shared_ptr<SDL_Renderer> target;
        void                          operator()(SDL_Texture* texture) const noexcept;
    };

    std::unique_ptr<SDL_Surface, SurfaceDeleter> pixels;
    std::string                                  description;
    ScaleMode                                    scaleMode;
    std::unique_ptr<SDL_Texture, TextureDeleter> gpuTexture;
};

#endif  // BITMAP_HPP
