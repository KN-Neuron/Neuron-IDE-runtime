#ifndef SCENE_HPP
#define SCENE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "scene/ImageLibrary.hpp"

class Delegate;
class SceneObject;
class SDL_Renderer;
struct Context;

class Scene {
   private:
    std::string                               experimentName;
    std::vector<std::shared_ptr<SceneObject>> objects;
    ImageLibrary                              imageLibrary;

   public:
    void setExperimentName(const std::string& name) { experimentName = name; }

    void addObject(std::shared_ptr<SceneObject> obj) { objects.push_back(std::move(obj)); }

    const std::string& getExperimentName() const { return experimentName; }
    const std::vector<std::shared_ptr<SceneObject>>& getObjects() const { return objects; }
    ImageLibrary&                                    images() { return imageLibrary; }

    void update(const Context& ctx);
    void render(SDL_Renderer* renderer);

    void onSceneReady();
    // Uploads the scene's images to `target`, then hands it to every component. Runs before the
    // first frame and again whenever the target has lost its textures.
    void      onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target);
    Delegate& resolveEvents(std::string_view objectName, std::string_view eventName);
};

#endif  // SCENE_HPP