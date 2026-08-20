#ifndef DELEGATE_HPP
#define DELEGATE_HPP

#include <cstddef>
#include <memory>

#include "events/EventListener.hpp"
#include "events/Subscription.hpp"

struct Context;

class Delegate {
   public:
    Delegate() : store(std::make_shared<ListenerStore>()) {}
    ~Delegate() = default;

    Delegate(const Delegate&)            = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&)                 = delete;
    Delegate& operator=(Delegate&&)      = delete;

    Subscription subscribe(EventListener* listener) {
        const std::size_t subscriptionId = store->nextId++;
        store->listeners.push_back({listener, subscriptionId});
        return Subscription(store, subscriptionId);
    }

    void invoke(const Context& ctx) {
        for (const auto& entry : store->listeners) {
            entry.listener->onEventTriggered(ctx);
        }
    }

   private:
    std::shared_ptr<ListenerStore> store;
};

#endif  // DELEGATE_HPP
