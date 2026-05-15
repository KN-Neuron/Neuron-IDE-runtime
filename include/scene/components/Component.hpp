#ifndef COMPONENT_HPP
#define COMPONENT_HPP

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = default;
    Component(Component&&) = default;
    Component& operator=(const Component&) = default;
    Component& operator=(Component&&) = default;
};

#endif // COMPONENT_H