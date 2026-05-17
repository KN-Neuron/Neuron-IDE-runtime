#ifndef BLINKCOMPONENT_HPP
#define BLINKCOMPONENT_HPP

#include <iostream>
#include <memory>

#include "Component.hpp"

namespace NeuronIDE {
class Component;
}

class BlinkComponent : public Component {
   public:
    BlinkComponent(double freq) : blinkFrequencyHz(freq) {
        std::cout << "    + [BlinkComponent] Utworzono z czestotliwoscia: " << blinkFrequencyHz
                  << "Hz\n";
    }
    void setFrequency(double freq);

    static std::unique_ptr<Component> createBlinker(const NeuronIDE::Component& protoComp);

   private:
    double blinkFrequencyHz = 0.0;
};

#endif  // BLINKCOMPONENT_HPP