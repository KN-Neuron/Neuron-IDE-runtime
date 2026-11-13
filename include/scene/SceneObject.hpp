#ifndef SCENEOBJECT_HPP
#define SCENEOBJECT_HPP

#include <memory>
#include <string>
#include <vector>

class Component;
class SDL_Renderer;
struct Context;

class SceneObject {
   public:
    std::string name;

    // Whether the object takes part in the scene at all. An inactive object is neither drawn nor
    // updated, and neither are its components. Set by the experiment file and by scripts; no
    // component writes it, so it is safe to attach appear/disappear events to its changes.
    bool isActive = true;

    struct Transform {
        double posX = 0, posY = 0, width = 0, height = 0, rotation = 0;

        // Throws std::invalid_argument, naming `who`, unless every value can be drawn with and the
        // width and height are positive.
        void requireDrawable(const std::string& who) const;
    } transform;

    std::vector<std::unique_ptr<Component>> components;

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
};

#endif  // SCENEOBJECT_HPP