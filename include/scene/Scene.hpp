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

// Always owned by a std::shared_ptr: scripts hold their scene weakly.
class Scene : public std::enable_shared_from_this<Scene> {
   private:
    std::string                               experimentName;
    std::vector<std::shared_ptr<SceneObject>> objects;
    ImageLibrary                              imageLibrary;

    // The frame being updated, or null between frames.
    const Context*           frame = nullptr;
    std::vector<std::string> heldMarkers;

   public:
    void setExperimentName(const std::string& name) { experimentName = name; }

    void addObject(std::shared_ptr<SceneObject> obj) { objects.push_back(std::move(obj)); }

    const std::string& getExperimentName() const { return experimentName; }
    const std::vector<std::shared_ptr<SceneObject>>& getObjects() const { return objects; }
    ImageLibrary&                                    images() { return imageLibrary; }

    void update(const Context& ctx);
    void render(SDL_Renderer* renderer);

    // Adds a marker to the frame being updated. One emitted between frames (by a script's
    // on_start) is held for the next frame, whether or not its object is active then.
    void emitMarker(std::string name);

    void onSceneReady();
    // Uploads the scene's images to `target`, then hands it to every component. Runs before the
    // first frame and again whenever the target has lost its textures.
    void      onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target);
    Delegate& resolveEvents(std::string_view objectName, std::string_view eventName);

    // The first object called `objectName`, or null.
    [[nodiscard]] std::shared_ptr<SceneObject> findObject(std::string_view objectName) const;
};

#endif  // SCENE_HPP