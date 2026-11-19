#ifndef LIFETIMECOMPONENT_HPP
#define LIFETIMECOMPONENT_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "events/Delegate.hpp"
#include "events/EventListener.hpp"
#include "events/Subscription.hpp"
#include "scene/components/Component.hpp"

class ComponentBuildContext;

namespace NeuronIDE {
class Component;
}

// Hides its object a set time after the object is activated, e.g. a cue shown for 200 ms.
//
// The countdown adds up Context::timestamp, so the object is hidden in the first frame in which
// the time passed reaches the duration. An object active from the start begins counting in its
// first frame. Lifetime only ever hides the object, never shows it, and it counts whatever other
// components do: a blinker on the same object does not pause it. After hiding the object the
// countdown starts over, so an object that is activated again gets the full duration. The same
// goes when something else (a script, say) hides the object first: the countdown is dropped when
// the object's "deactivate" fires, so the time already counted is not carried over.
//
// Exposes the event "expire" through getEvent(). It fires in the frame Lifetime hides the object,
// next to the object's own "deactivate", and never when something else hides the object early, so a
// MarkerEmitter or a script can tell a timeout from an early hide.
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

    // The event "expire", or null for any other name.
    Delegate* getEvent(std::string_view eventName) override;

    [[nodiscard]] std::uint32_t durationMs() const { return duration; }

    static std::unique_ptr<Component> createLifetime(const NeuronIDE::Component&         protoComp,
                                                     const std::shared_ptr<SceneObject>& owner,
                                                     const ComponentBuildContext&        context);

   private:
    // Starts the countdown over when the owner is deactivated.
    class DropCountdown : public EventListener {
       public:
        explicit DropCountdown(LifetimeComponent* lifetime) : lifetime(lifetime) {}

        void onEventTriggered(const Context& context) override {
            (void)context;
            lifetime->elapsedSeconds = 0.0;
        }

       private:
        LifetimeComponent* lifetime;
    };

    std::uint32_t duration;
    double        elapsedSeconds = 0.0;
    Delegate      expireEvent;
    DropCountdown dropCountdown{this};
    // Declared after the listener it points to, so it unsubscribes first.
    std::optional<Subscription> deactivateSubscription;
};

#endif  // LIFETIMECOMPONENT_HPP
