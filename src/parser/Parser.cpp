#include "parser/Parser.hpp"

#include <fstream>
#include <stdexcept>
#include <unordered_set>

#include "neuronide.pb.h"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"
#include "scene/components/ComponentRegistry.hpp"

std::shared_ptr<Scene> Parser::parse(const std::string& filePath) {
    NeuronIDE::Scene protoScene;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Parser: cannot open file: " + filePath);
    }

    if (!protoScene.ParseFromIstream(&file)) {
        throw std::runtime_error("Parser: failed to parse protobuf from: " + filePath);
    }

    auto scene = std::make_shared<::Scene>();
    scene->setExperimentName(protoScene.project_name());

    for (const auto& protoObj : protoScene.scene_objects()) {
        auto obj = buildSceneObject(protoObj);
        scene->addObject(std::move(obj));
    }

    return scene;
}

std::shared_ptr<SceneObject> Parser::buildSceneObject(const NeuronIDE::SceneObject& protoObj) {
    auto obj = std::make_shared<SceneObject>(protoObj.name(), protoObj.is_visible());

    if (protoObj.has_transform()) {
        const auto& tra = protoObj.transform();
        obj->setTransform({tra.x(), tra.y(), tra.width(), tra.height(), tra.rotation()});
    }

    std::unordered_set<int> seenComponentTypes;

    for (const auto& protoComp : protoObj.components()) {
        int typeId = static_cast<int>(protoComp.component_type_case());
        
        if (seenComponentTypes.find(typeId) != seenComponentTypes.end()) {
            throw std::runtime_error("Parser: duplicate component type in object '" + protoObj.name() + "'.");
        }
        seenComponentTypes.insert(typeId);

        auto comp = buildComponent(protoComp);
        if (comp) {
            obj->addComponent(std::move(comp));
        }
    }

    return obj;
}

std::unique_ptr<Component> Parser::buildComponent(const NeuronIDE::Component& protoComp) {
    return ComponentRegistry::instance().build(protoComp);
}