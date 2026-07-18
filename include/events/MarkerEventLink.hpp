#ifndef MARKEREVENTLINK_HPP
#define MARKEREVENTLINK_HPP

#include <string>
#include <utility>

#include "data_structures/Context.hpp"
#include "events/EventListener.hpp"

class MarkerEventLink : public EventListener {
   public:
    explicit MarkerEventLink(std::string name) : eventName(std::move(name)) {}

    void onEventTriggered(const Context& ctx) override {
        if (ctx.markers != nullptr) {
            ctx.markers->push_back(eventName);
        }
    }

   private:
    std::string eventName;
};

#endif  // MARKEREVENTLINK_HPP
