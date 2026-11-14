#include "scene/Scene.hpp"

#include <stdexcept>
#include <utility>

#include "data_structures/Context.hpp"
#include "events/Delegate.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"

namespace {

// Points `current` at the frame being updated for as long as it lives.
class FrameScope {
   public:
    FrameScope(const Context*& current, const Context& frame) : current(&current) {
        current = &frame;
    }
    ~FrameScope() { *current = nullptr; }

    FrameScope(const FrameScope&)            = delete;
    FrameScope& operator=(const FrameScope&) = delete;
    FrameScope(FrameScope&&)                 = delete;
    FrameScope& operator=(FrameScope&&)      = delete;

   private:
    const Context** current;
};

}  // namespace

void Scene::update(const Context& ctx) {
    if (ctx.markers != nullptr) {
        ctx.markers->insert(ctx.markers->end(), heldMarkers.begin(), heldMarkers.end());
    }
    heldMarkers.clear();

    const FrameScope scope(frame, ctx);
    for (const auto& obj : objects) {
        obj->update(ctx);
    }
}

void Scene::emitMarker(std::string name) {
    if (frame == nullptr) {
        heldMarkers.push_back(std::move(name));
    } else if (frame->markers != nullptr) {
        frame->markers->push_back(std::move(name));
    }
}

void Scene::render(SDL_Renderer* renderer) {
    for (const auto& obj : objects) {
        obj->render(renderer);
    }
}

void Scene::onSceneReady() {
    for (const auto& obj : objects) {
        for (const auto& comp : obj->components) {
            comp->onSceneReady(*this);
        }
    }
}

void Scene::onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) {
    imageLibrary.upload(target);
    for (const auto& obj : objects) {
        for (const auto& comp : obj->components) {
            comp->onRenderTargetReady(target);
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

std::shared_ptr<SceneObject> Scene::findObject(std::string_view objectName) const {
    for (const auto& obj : objects) {
        if (obj->name == objectName) {
            return obj;
        }
    }
    return nullptr;
}