#ifndef COMPONENT_HPP
#define COMPONENT_HPP

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = default;
    Component(Component&&) = default;
    auto operator=(const Component&) -> Component& = default;
    auto operator=(Component&&) -> Component& = default;
};

#endif // COMPONENT_H