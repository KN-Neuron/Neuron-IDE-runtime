#ifndef MARKEREMITTERCOMPONENT_HPP
#define MARKEREMITTERCOMPONENT_HPP

#include <memory>
#include <string>
#include <vector>

#include "events/MarkerEventLink.hpp"
#include "events/Subscription.hpp"
#include "scene/components/Component.hpp"

class Scene;

namespace NeuronIDE {
class Component;
}

class MarkerEmitterComponent : public Component {
   public:
    struct Binding {
        std::string targetObject;
        std::string targetEvent;
        std::string markerName;
    };

    MarkerEmitterComponent(const std::shared_ptr<SceneObject>& owner,
                           std::vector<Binding>                bindings);
    ~MarkerEmitterComponent() override = default;

    MarkerEmitterComponent(const MarkerEmitterComponent&)            = delete;
    MarkerEmitterComponent& operator=(const MarkerEmitterComponent&) = delete;
    MarkerEmitterComponent(MarkerEmitterComponent&&)                 = delete;
    MarkerEmitterComponent& operator=(MarkerEmitterComponent&&)      = delete;

    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;
    void onSceneReady(Scene& scene) override;

    static std::unique_ptr<Component> createMarkerEmitter(
        const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner);

   private:
    struct ActiveSubscription {
        // Declaration order matters: the Subscription is destroyed first, unsubscribing before
        // the listener it points to is freed.
        std::unique_ptr<MarkerEventLink> link;
        Subscription                     subscription;
    };

    std::vector<Binding>            bindings;
    std::vector<ActiveSubscription> activeSubscriptions;
};

#endif  // MARKEREMITTERCOMPONENT_HPP
