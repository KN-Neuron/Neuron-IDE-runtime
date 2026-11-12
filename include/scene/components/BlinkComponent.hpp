#ifndef BLINKCOMPONENT_HPP
#define BLINKCOMPONENT_HPP

#include "Component.hpp"

namespace NeuronIDE {
class Component;
}
class ComponentBuildContext;

// Flickers its owner at `blinkFrequencyHz`. The component only keeps its own lit/unlit state and
// reports it through allowsDrawing(); it never writes to the owner, so nothing else that controls
// the object (a script, an appear/disappear event) is overridden by the blinking.
class BlinkComponent : public Component {
   public:
    BlinkComponent(const std::shared_ptr<SceneObject>& owner, double freq)
        : Component(owner), blinkFrequencyHz(freq) {}

    // Restarts the blink cycle, so the new frequency begins lit. A frequency that is 0, negative or
    // not finite stops the blinking and leaves the component lit.
    void setFrequency(double freq);

    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;
    bool allowsDrawing() const override { return isLit; }

    static std::unique_ptr<Component> createBlinker(const NeuronIDE::Component&         protoComp,
                                                    const std::shared_ptr<SceneObject>& owner,
                                                    const ComponentBuildContext&        context);

   private:
    double blinkFrequencyHz = 0.0;
    double elapsedTime      = 0.0;
    bool   isLit            = true;
};

#endif  // BLINKCOMPONENT_HPP
