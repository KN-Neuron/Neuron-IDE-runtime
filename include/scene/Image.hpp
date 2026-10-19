#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>
#include <memory>

struct SDL_Surface;
struct SDL_Texture;
class SDL_Renderer;

// An image file decoded into memory, plus its texture once uploaded to a render target. The
// decoded pixels are kept after the upload so the texture can be rebuilt when the render target
// loses it (SDL_RENDER_DEVICE_RESET).
class Image {
   public:
    // Throws std::runtime_error when the file cannot be opened or decoded.
    explicit Image(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return filePath; }

    // Creates the texture on `target`, replacing any earlier one. Throws std::invalid_argument for
    // a null target and std::runtime_error when SDL cannot create the texture, e.g. when the image
    // is larger than the GPU's maximum texture size.
    void upload(const std::shared_ptr<SDL_Renderer>& target);

    // Null until upload() has succeeded.
    [[nodiscard]] SDL_Texture* texture() const noexcept { return gpuTexture.get(); }

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

    std::filesystem::path                        filePath;
    std::unique_ptr<SDL_Surface, SurfaceDeleter> pixels;
    std::unique_ptr<SDL_Texture, TextureDeleter> gpuTexture;
};

#endif  // IMAGE_HPP
