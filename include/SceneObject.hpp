#ifndef SCENEOBJECT_HPP
#define SCENEOBJECT_HPP

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "components/Component.hpp"

class SceneObject {
public:
    std::string name;
    bool isVisible = true;
    
    // Struktura trzymająca pozycję
    struct Transform {
        double posX = 0, posY = 0, width = 0, height = 0, rotation = 0;
    } transform;

    std::vector<std::unique_ptr<Component>> components;

    SceneObject(std::string n, bool visible = true) 
        : name(std::move(n)), isVisible(visible) {
        std::cout << "  [SceneObject] Utworzono obiekt: " << name << "\n";
    }

    void setTransform(double posX, double posY, double width, double height, double rotation) {
        transform = {posX, posY, width, height, rotation};
    }

    void addComponent(std::unique_ptr<Component> comp) {
        components.push_back(std::move(comp));
    }
};

#endif // SCENEOBJECT_HPP