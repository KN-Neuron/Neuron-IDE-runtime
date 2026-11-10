#include "scene/components/TextRenderer.hpp"

#include <SDL2/SDL.h>

#include <cmath>
#include <limits>
#include <logging/Logging.hpp>
#include <stdexcept>
#include <string>

#include "Utf8Path.hpp"
#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentBuildContext.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
constexpr double kHalf = 0.5;

quill::Logger* logger() { return logging::logger(logging::Subsystem::Scene); }

std::string describe(const SceneObject& owner) { return "TextRenderer on '" + owner.name + "'"; }

// Keeps the wrap width a valid int, and the centre computed from these within float range.
bool fitsInInt(double value) {
    return std::abs(value) <= static_cast<double>(std::numeric_limits<int>::max());
}
}  // namespace

TextRenderer::TextRenderer(const std::shared_ptr<SceneObject>& owner, std::string_view initialText,
                           const std::filesystem::path& fontPath, std::uint32_t fontSize)
    : Component(owner), text(initialText) {
    if (!owner) {
        throw std::invalid_argument("TextRenderer: owner must not be null");
    }

    const std::string who       = describe(*owner);
    const auto&       transform = owner->transform;
    transform.requireDrawable(who);
    for (const double value : {transform.posX, transform.posY, transform.width, transform.height}) {
        if (!fitsInInt(value)) {
            throw std::invalid_argument(who + ": the transform does not fit in whole pixels");
        }
    }
    // SDL_ttf stops working reliably far above any size a display can show.
    if (fontSize == 0 || fontSize > kMaxFontSize) {
        throw std::invalid_argument(who + ": the font size must be between 1 and " +
                                    std::to_string(kMaxFontSize) + ", got " +
                                    std::to_string(fontSize));
    }

    font      = std::make_unique<Font>(fontPath, static_cast<int>(fontSize));
    wrapWidth = static_cast<int>(std::ceil(transform.width));
    bitmap    = font->render(text, wrapWidth);

    if (!bitmap) {
        LOG_WARNING(logger(), "{} has no text and draws nothing", who);
    } else if (static_cast<double>(bitmap->height()) > std::ceil(transform.height) ||
               font->widestWord(text) > wrapWidth) {
        LOG_WARNING(logger(),
                    "{}: the text at font size {} does not fit its {}x{} box, so words may be "
                    "split or run past the box",
                    who, fontSize, transform.width, transform.height);
    }
}

void TextRenderer::setText(std::string_view newText) {
    if (newText == text) {
        return;
    }
    // Built aside, so a throw leaves the old text in place.
    std::optional<Bitmap> next = font->render(newText, wrapWidth);
    if (next && renderTarget) {
        next->upload(renderTarget);
    }
    bitmap = std::move(next);
    text   = newText;
}

void TextRenderer::update(const Context& context) { (void)context; }

void TextRenderer::render(SDL_Renderer* renderer) {
    const auto ownerPtr = owner.lock();
    if (renderer == nullptr || !bitmap) {
        return;
    }
    if (bitmap->texture() == nullptr || !ownerPtr) {
        // render() runs every frame, so this is reported once per text.
        if (!missingStimulusReported) {
            missingStimulusReported = true;
            LOG_WARNING(logger(), "A text stimulus is missing from the screen: {}",
                        ownerPtr ? describe(*ownerPtr) + " has no texture" : "its object is gone");
        }
        return;
    }

    const auto&  transform = ownerPtr->transform;
    const double width     = bitmap->width();
    const double height    = bitmap->height();
    // Whole pixels, because glyphs drawn between two pixels lose their sharp edges.
    const SDL_FRect destination{
        static_cast<float>(std::round(transform.posX + ((transform.width - width) * kHalf))),
        static_cast<float>(std::round(transform.posY + ((transform.height - height) * kHalf))),
        static_cast<float>(width), static_cast<float>(height)};

    // A null centre rotates about the centre of `destination`, which is the rectangle's centre.
    if (SDL_RenderCopyExF(renderer, bitmap->texture(), nullptr, &destination, transform.rotation,
                          nullptr, SDL_FLIP_NONE) != 0) {
        throw std::runtime_error(describe(*ownerPtr) + ": cannot draw the text: " + SDL_GetError());
    }
}

void TextRenderer::onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) {
    renderTarget = target;
    if (bitmap) {
        bitmap->upload(target);
    }
}

std::unique_ptr<Component> TextRenderer::createText(const NeuronIDE::Component&         protoComp,
                                                    const std::shared_ptr<SceneObject>& owner,
                                                    const ComponentBuildContext&        context) {
    const auto& proto = protoComp.text();
    return std::make_unique<TextRenderer>(owner, proto.text(),
                                          context.resolveAsset(utf8::toPath(proto.font_path())),
                                          proto.font_size());
}

REGISTER_COMPONENT(NeuronIDE::Component::kText, TextRenderer::createText)
