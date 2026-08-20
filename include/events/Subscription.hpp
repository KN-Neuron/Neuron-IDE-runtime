#ifndef SUBSCRIPTION_HPP
#define SUBSCRIPTION_HPP

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

class EventListener;

// Shared listener bookkeeping. A Delegate owns one of these via std::shared_ptr and hands a
// std::weak_ptr to each Subscription it creates, so a Subscription can detect when its Delegate
// has already been destroyed and skip unsubscribing (a no-op instead of UB).
struct ListenerStore {
    struct Entry {
        EventListener* listener;
        std::size_t    subscriptionId;
    };

    std::vector<Entry> listeners;
    std::size_t        nextId = 0;
};

// RAII handle returned by Delegate::subscribe. Unsubscribes from its Delegate on destruction;
// if the Delegate is already gone the operation is a no-op.
class Subscription {
   public:
    Subscription(std::weak_ptr<ListenerStore> store, std::size_t subscriptionId)
        : store(std::move(store)), subscriptionId(subscriptionId) {}

    ~Subscription() {
        if (auto lockedStore = store.lock()) {
            std::erase_if(lockedStore->listeners,
                          [subscriptionId = subscriptionId](const ListenerStore::Entry& entry) {
                              return entry.subscriptionId == subscriptionId;
                          });
        }
    }

    Subscription(const Subscription&)                = delete;
    Subscription& operator=(const Subscription&)     = delete;
    Subscription(Subscription&&) noexcept            = default;
    Subscription& operator=(Subscription&&) noexcept = default;

   private:
    std::weak_ptr<ListenerStore> store;
    std::size_t                  subscriptionId;
};

#endif  // SUBSCRIPTION_HPP
