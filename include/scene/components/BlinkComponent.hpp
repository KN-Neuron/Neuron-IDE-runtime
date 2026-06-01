#ifndef BLINKCOMPONENT_HPP
#define BLINKCOMPONENT_HPP

#include <iostream>

#include "Component.hpp"

namespace NeuronIDE {
class Component;
}

class BlinkComponent : public Component {
   public:
    BlinkComponent(std::shared_ptr<SceneObject> owner, double freq) : Component(owner), blinkFrequencyHz(freq) {}
    void setFrequency(double freq);

    void update(const Context& context) override;

    static std::unique_ptr<Component> createBlinker(const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner);

   private:
    double blinkFrequencyHz = 0.0;
};

#endif  // BLINKCOMPONENT_HPP