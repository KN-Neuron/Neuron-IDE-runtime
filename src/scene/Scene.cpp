#include "scene/Scene.hpp"

#include "scene/SceneObject.hpp"

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