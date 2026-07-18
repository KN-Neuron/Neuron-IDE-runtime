#include "scene/Scene.hpp"

#include <stdexcept>

#include "events/Delegate.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"

void Scene::update(const Context& ctx) {
    for (const auto& obj : objects) {
        obj->update(ctx);
    }
}

void Scene::render(SDL_Renderer* renderer) {
    for (const auto& obj : objects) {
        if (obj->isVisible) {
            obj->render(renderer);
        }
    }
}

void Scene::onSceneReady() {
    for (const auto& obj : objects) {
        for (const auto& comp : obj->components) {
            comp->onSceneReady(*this);
        }
    }
}

Delegate& Scene::resolveEvents(std::string_view objectName, std::string_view eventName) {
    for (const auto& obj : objects) {
        if (obj->name != objectName) {
            continue;
        }
        for (const auto& comp : obj->components) {
            auto* delegate = comp->getEvent(eventName);
            if (delegate != nullptr) {
                return *delegate;
            }
        }
        throw std::runtime_error("Scene::resolveEvents: event '" + std::string(eventName) +
                                 "' not found on object '" + std::string(objectName) + "'");
    }
    throw std::runtime_error("Scene::resolveEvents: object '" + std::string(objectName) +
                             "' not found");
}