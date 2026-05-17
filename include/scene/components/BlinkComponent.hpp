#ifndef BLINKCOMPONENT_HPP
#define BLINKCOMPONENT_HPP

#include <iostream>

#include "Component.hpp"

class BlinkComponent : public Component {
   public:
    BlinkComponent(double freq) : blinkFrequencyHz(freq) {
        std::cout << "    + [BlinkComponent] Utworzono z czestotliwoscia: " << blinkFrequencyHz
                  << "Hz\n";
    }
    void setFrequency(double freq) { blinkFrequencyHz = freq; }

   private:
    double blinkFrequencyHz = 0.0;
};

#endif  // BLINKCOMPONENT_HPP