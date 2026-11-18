#ifndef LIFETIMECOMPONENT_HPP
#define LIFETIMECOMPONENT_HPP

#include <cstdint>
#include <memory>

#include "scene/components/Component.hpp"

class ComponentBuildContext;

namespace NeuronIDE {
class Component;
}

// Hides its object a set time after the object is activated, e.g. a cue shown for 200 ms.
class LifetimeComponent : public Component {
   public:
    // The longest duration accepted: 24 hours. Anything above is almost certainly a mistake, such
    // as a value written in microseconds.
    static constexpr std::uint32_t kMaxDurationMs = 86'400'000;

    // Throws std::invalid_argument for a null owner and for a duration that is 0 or longer than
    // kMaxDurationMs.
    LifetimeComponent(const std::shared_ptr<SceneObject>& owner, std::uint32_t milliseconds);
    ~LifetimeComponent() override = default;

    LifetimeComponent(const LifetimeComponent&)            = delete;
    LifetimeComponent& operator=(const LifetimeComponent&) = delete;
    LifetimeComponent(LifetimeComponent&&)                 = delete;
    LifetimeComponent& operator=(LifetimeComponent&&)      = delete;

    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;

    [[nodiscard]] std::uint32_t durationMs() const { return duration; }

    static std::unique_ptr<Component> createLifetime(const NeuronIDE::Component&         protoComp,
                                                     const std::shared_ptr<SceneObject>& owner,
                                                     const ComponentBuildContext&        context);

   private:
    std::uint32_t duration;
};

#endif  // LIFETIMECOMPONENT_HPP
