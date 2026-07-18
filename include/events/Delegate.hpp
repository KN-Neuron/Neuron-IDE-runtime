#ifndef DELEGATE_HPP
#define DELEGATE_HPP

#include <algorithm>
#include <vector>

#include "events/EventListener.hpp"

struct Context;

class Delegate {
   public:
    Delegate()  = default;
    ~Delegate() = default;

    Delegate(const Delegate&)            = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&)                 = delete;
    Delegate& operator=(Delegate&&)      = delete;

    void subscribe(EventListener* listener) { listeners.push_back(listener); }

    void unsubscribe(EventListener* listener) {
        listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
    }

    void invoke(const Context& ctx) {
        for (auto* listener : listeners) {
            listener->onEventTriggered(ctx);
        }
    }

   private:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    std::vector<EventListener*> listeners;
};

#endif  // DELEGATE_HPP
