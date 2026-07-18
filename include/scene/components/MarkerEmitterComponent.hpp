#ifndef MARKEREMITTERCOMPONENT_HPP
#define MARKEREMITTERCOMPONENT_HPP

#include <iostream>

#include "Component.hpp"

namespace NeuronIDE {
class Component;
}

class MarkerEmitterComponent : public Component {
   public:
    void update(const Context& context) override;
    void render(SDL_Renderer* renderer) override;
};

#endif  // BLINKCOMPONENT_HPP