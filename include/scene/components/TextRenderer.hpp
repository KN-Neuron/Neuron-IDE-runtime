#ifndef TEXTRENDERER_HPP
#define TEXTRENDERER_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "scene/Bitmap.hpp"
#include "scene/Font.hpp"
#include "scene/components/Component.hpp"

class ComponentBuildContext;

namespace NeuronIDE {
class Component;
}

// Draws white text centred in its owner's Transform and rotated, in degrees clockwise, about the
// rectangle's centre. The font size is the size on screen: the text is never stretched to the
// rectangle. It wraps at the rectangle's width, and text taller than the rectangle runs past it.
//
// The whole text is one texture, drawn in one call, so a frame costs the same however long the
// text is. It is rendered while the scene is parsed, so a missing font fails before any window
// opens, and uploaded before the first frame.
class TextRenderer : public Component {
   public:
    static constexpr std::uint32_t kMaxFontSize = 4096;

    // An empty `fontPath` selects the built-in font. Throws std::invalid_argument for a null owner,
    // a font size outside 1..kMaxFontSize, or a transform that is not finite, does not fit in whole
    // pixels or has no positive width and height, and std::runtime_error when the font cannot be
    // loaded or the text rendered.
    TextRenderer(const std::shared_ptr<SceneObject>& owner, std::string_view initialText,
                 const std::filesystem::path& fontPath, std::uint32_t fontSize);

    // Renders and uploads the new text straight away, so call it on the render thread, before the
    // frame is drawn. Costs nothing when the text is unchanged. Throws what the constructor throws
    // for text that cannot be rendered, and std::runtime_error when it cannot be uploaded.
    void setText(std::string_view newText);

    void update(const Context& context) override;

    // Draws nothing while the text has no texture yet. Throws std::runtime_error when SDL fails to
    // draw it, so a frame without its stimulus is never presented and marked as shown.
    void render(SDL_Renderer* renderer) override;

    void onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) override;

    static std::unique_ptr<Component> createText(const NeuronIDE::Component&         protoComp,
                                                 const std::shared_ptr<SceneObject>& owner,
                                                 const ComponentBuildContext&        context);

   private:
    void rasterize();

    std::unique_ptr<Font>         font;
    std::string                   text;
    int                           wrapWidth = 0;
    std::optional<Bitmap>         bitmap;
    std::shared_ptr<SDL_Renderer> renderTarget;
    bool                          missingStimulusReported = false;
};

#endif  // TEXTRENDERER_HPP
