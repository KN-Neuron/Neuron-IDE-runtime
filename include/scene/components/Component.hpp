#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include <memory>

class SceneObject;
class SDL_Renderer;
struct Context;

class Component {
   public:
    Component() = delete;
    Component(const std::shared_ptr<SceneObject>& owner) : owner(owner) {}
    virtual ~Component() = default;

    Component(const Component&)            = delete;
    Component(Component&&)                 = delete;
    Component& operator=(const Component&) = delete;
    Component& operator=(Component&&)      = delete;

    virtual void update(const Context& context) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;

   protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    std::weak_ptr<SceneObject> owner;
};

#endif  // COMPONENT_HPP