
#include "defines.h"
#include "logger.h"
#include "memory.h"

#define MAX_EVENT_ID 1024
#define MAX_LISTENER_COUNT 4
#define LISTENER_BITMASK_MAX 15

typedef b8 (*PFN_on_event)(u32 event_id, void *sender, void *listener);

struct RegisteredListener
{
    void *ptr;
    PFN_on_event callback;
};

struct platformEvent
{
    u32 listenerMask;
    struct RegisteredListener listeners[MAX_LISTENER_COUNT];
};

static struct platformEvent *registered_events; 

struct platformEvent *event_read_pointer;
struct platformEvent *event_write_pointer;

void initialise_event_handler()
{
    registered_events = (struct platformEvent *)platform_alloc(MAX_EVENT_ID * sizeof(struct platformEvent),
                                                             MEM_TAG_EVENT_SYSTEM); // Static allocation for testing, make dynamic later.
}

void signal_event(u32 event_id, void *sender)
{
    if (registered_events[event_id].listenerMask == 0)
    {
        return;
    }
    for (u32 i = 0; i < MAX_LISTENER_COUNT; i++)
    {
        if (registered_events[event_id].listeners[i].callback(event_id, sender, registered_events[event_id].listeners[i].ptr))
        {
            return;
        }
    }
}

void register_event_listen(u32 event_id, void *listener, PFN_on_event on_event)
{
    if (event_id >= MAX_EVENT_ID)
    {
        PERROR("Tried to register an invalid event ID: %d", event_id);
        return;
    }
    for (i32 i = 0; i < MAX_LISTENER_COUNT; i++)
    {
        if (registered_events[event_id].listeners[i].ptr == listener)
        {
            PERROR("Tried to register the a listener twice for the same event");
            return;
        }
    }

    // NOTE: This is a test of a case switch allocator idea
    switch (registered_events[event_id].listenerMask & 15)
    {
    case 0:
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
        registered_events[event_id].listenerMask |= (1 << 0);

        registered_events[event_id].listeners[0].ptr = listener;
        registered_events[event_id].listeners[0].callback = on_event;
        break;

    case 1:
    case 5:
    case 9:
    case 13:
        registered_events[event_id].listenerMask |= (1 << 1);

        registered_events[event_id].listeners[1].ptr = listener;
        registered_events[event_id].listeners[1].callback = on_event;
        break;

    case 3:
    case 11:
        registered_events[event_id].listenerMask |= (1 << 2);

        registered_events[event_id].listeners[2].ptr = listener;
        registered_events[event_id].listeners[2].callback = on_event;
        break;

    case 7:
        registered_events[event_id].listenerMask |= (1 << 3);

        registered_events[event_id].listeners[3].ptr = listener;
        registered_events[event_id].listeners[3].callback = on_event;
        break;

    default:
        PERROR("Tried to allocate too many listeners to event %d", event_id);
    }
    return;
}

void unregister_event_listen(u32 event_id, void *listener)
{
    if (event_id >= MAX_EVENT_ID)
    {
        PERROR("Tried to unregister a listener from an invalid event id.");
        return;
    }

    for (i32 i = 0; i < MAX_LISTENER_COUNT; i++)
    {
        if (registered_events[event_id].listeners[i].ptr == listener)
        {
            u32 bitmask = LISTENER_BITMASK_MAX - (1 << i);
            registered_events[event_id].listenerMask &= bitmask;
            return;
        }
    }
    PWARN("Tried to unregister a listener which was not registered");
    return;
}