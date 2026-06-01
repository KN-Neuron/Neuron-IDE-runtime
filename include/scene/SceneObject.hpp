#ifndef SCENEOBJECT_HPP
#define SCENEOBJECT_HPP

#include <iostream>
#include <memory>
#include <string>
#include <vector>

class Component;
class SDL_Renderer;
struct Context;

class SceneObject {
   public:
    std::string name;
    bool        isVisible = true;

    struct Transform {
        double posX = 0, posY = 0, width = 0, height = 0, rotation = 0;
    } transform;

    std::vector<std::unique_ptr<Component>> components;

    SceneObject(std::string n, bool visible = true);

    void setTransform(const Transform& transform);

    void addComponent(std::unique_ptr<Component> comp);

    void update(const Context& ctx);
    void render(SDL_Renderer* renderer);
};

#endif  // SCENEOBJECT_HPP