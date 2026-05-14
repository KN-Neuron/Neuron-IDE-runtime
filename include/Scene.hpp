#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include <memory>
#include "SceneObject.hpp"

class Scene {
private:
    std::string experimentName;
    std::vector<std::shared_ptr<SceneObject>> objects;

public:
    void setExperimentName(const std::string& name) {
        experimentName = name;
    }

    void addObject(std::shared_ptr<SceneObject> obj) {
        objects.push_back(std::move(obj));
    }

    const std::string& getExperimentName() const { return experimentName; }
    const std::vector<std::shared_ptr<SceneObject>>& getObjects() const { return objects; }
};

#endif // SCENE_H