#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include <memory>
#include <string_view>

class Delegate;
class Scene;
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

    virtual void onSceneReady(Scene& scene) { (void)scene; }

    // Called once the render target this scene is drawn to exists, before the first frame, and
    // again whenever the target has lost its textures (SDL_RENDER_DEVICE_RESET): the place to
    // (re)create GPU resources. SDL frees a renderer's textures along with it, so a component
    // holding any keeps `target` alive for as long as it does.
    virtual void onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) { (void)target; }

    virtual Delegate* getEvent(std::string_view eventName) {
        (void)eventName;
        return nullptr;
    }

   protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    std::weak_ptr<SceneObject> owner;
};

#endif  // COMPONENT_HPP