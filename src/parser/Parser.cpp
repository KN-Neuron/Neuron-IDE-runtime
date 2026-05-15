#include "parser/Parser.hpp"

#include "scene/SceneAll.hpp"

#include <google/protobuf/text_format.h>  // dla .pbtxt jeśli będzie potrzeba

#include <fstream>
#include <stdexcept>


#include "neuronide.pb.h"  // wygenerowany przez protoc

std::shared_ptr<::Scene> Parser::parse(const std::string& filePath) {
    NeuronIDE::Scene protoScene;

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Parser: cannot open file: " + filePath);
    }

    if (!protoScene.ParseFromIstream(&file)) {
        throw std::runtime_error("Parser: failed to parse protobuf from: " + filePath);
    }

    // Budowanie natywnej sceny silnika
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

    // Transform (jeśli wyciągnięty poza oneof)
    if (protoObj.has_transform()) {
        const auto& tra = protoObj.transform();
        obj->setTransform(tra.x(), tra.y(), tra.width(), tra.height(), tra.rotation());
    }

    for (const auto& protoComp : protoObj.components()) {
        auto comp = buildComponent(protoComp);
        if (comp) {
            obj->addComponent(std::move(comp));
        }
    }

    return obj;
}

std::unique_ptr<Component> Parser::buildComponent(const NeuronIDE::Component& protoComp) {
    using CT = NeuronIDE::Component::ComponentTypeCase;

    switch (protoComp.component_type_case()) {
        // case CT::kRenderer: {
        //     const auto& ren = protoComp.renderer();
        //     return std::make_unique<SpriteRenderer>(ren.texture_path(), ren.img_path());
        // }

        // case CT::kText: {
        //     const auto& txt = protoComp.text();
        //     return std::make_unique<TextRenderer>(txt.text(), txt.font_path(), txt.font_size());
        // }

        case CT::kBlinker: {
            const auto& bli = protoComp.blinker();
            return std::make_unique<BlinkComponent>(bli.blink_frequency_hz());
        }

        // case CT::kScript: {
        //     const auto& scr = protoComp.script();
        //     return std::make_unique<ScriptComponent>(scr.script_path());
        // }

        default:
            return nullptr;
    }
}