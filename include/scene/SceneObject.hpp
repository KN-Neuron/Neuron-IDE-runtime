#ifndef SCENEOBJECT_HPP
#define SCENEOBJECT_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "events/Delegate.hpp"

class Component;
class SDL_Renderer;
struct Context;

class SceneObject {
   public:
    // These fields predate the private event state below. Hiding them behind accessors is a
    // refactor of its own.
    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    std::string name;

    // Whether the object takes part in the scene at all. An inactive object is neither drawn nor
    // updated, and neither are its components. Set by the experiment file and by scripts; no
    // component writes it, so it is safe to attach the activate/deactivate events to its changes.
    bool isActive = true;

    struct Transform {
        double posX = 0, posY = 0, width = 0, height = 0, rotation = 0;

        // Throws std::invalid_argument, naming `who`, unless every value can be drawn with and the
        // width and height are positive.
        void requireDrawable(const std::string& who) const;
    } transform;

    std::vector<std::unique_ptr<Component>> components;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

    SceneObject(std::string n, bool active = true);

    void setTransform(const Transform& transform);

    void addComponent(std::unique_ptr<Component> comp);

    // The first component of type T, or null.
    template <typename T>
    [[nodiscard]] T* findComponent() const {
        for (const auto& comp : components) {
            if (auto* found = dynamic_cast<T*>(comp.get())) {
                return found;
            }
        }
        return nullptr;
    }

    void update(const Context& ctx);
    void render(SDL_Renderer* renderer);

    // The event "activate" or "deactivate", or null for any other name. "activate" fires in the
    // first frame the object is active, "deactivate" in the first frame it is inactive again.
    Delegate* getEvent(std::string_view eventName);

    // Fires "activate" or "deactivate" if isActive has changed since the last call. Scene::update
    // calls it after updating every object, so a change made anywhere in a frame fires in it.
    void dispatchActivateStatusChange(const Context& ctx);

   private:
    Delegate onActivate;
    Delegate onDeactivate;

    // Nothing is on screen before the first frame: an object active from the start fires
    // "activate" in it, and one inactive from the start fires nothing.
    bool wasActive = false;
};

#endif  // SCENEOBJECT_HPP
