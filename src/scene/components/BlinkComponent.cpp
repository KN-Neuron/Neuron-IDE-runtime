#include "scene/components/BlinkComponent.hpp"

#include "neuronide.pb.h"
#include "scene/components/ComponentRegistry.hpp"

void BlinkComponent::setFrequency(double freq) { blinkFrequencyHz = freq; }

std::unique_ptr<Component> BlinkComponent::createBlinker(const NeuronIDE::Component& protoComp) {
    return std::make_unique<BlinkComponent>(protoComp.blinker().blink_frequency_hz());
}

// Anonymous namespace to register the BlinkComponent creator in the ComponentRegistry at program startup
namespace {
struct BlinkRegistrar {
    BlinkRegistrar() {
        ComponentRegistry::instance().registerCreator(
            static_cast<int>(NeuronIDE::Component::kBlinker), BlinkComponent::createBlinker);
    }
};

static BlinkRegistrar global_blink_registrar;
}  // namespace