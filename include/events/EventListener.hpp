#ifndef EVENTLISTENER_HPP
#define EVENTLISTENER_HPP

struct Context;

class EventListener {
   public:
    EventListener()  = default;
    virtual ~EventListener() = default;

    EventListener(const EventListener&)            = delete;
    EventListener& operator=(const EventListener&) = delete;
    EventListener(EventListener&&)                 = delete;
    EventListener& operator=(EventListener&&)      = delete;

    virtual void onEventTriggered(const Context& ctx) = 0;
};

#endif  // EVENTLISTENER_HPP
