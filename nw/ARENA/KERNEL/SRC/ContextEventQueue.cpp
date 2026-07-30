#include "kernel/h/context.h"

// Context.cpp owns the queue layout. Repeating the tagged definition here
// keeps the legacy source byte-for-byte intact while allowing modern,
// read-only queue inspection and destination-routed removal.
struct KR_EventListElemName
{
    KR_Event *event;
    int next;
};

namespace {

const int kEndList = -1;

}  // namespace

int SimulationContext::copyAllEvents(KR_Event *events, int capacity) const
{
    if (capacity < 0 || (capacity > 0 && events == NULL))
        return -1;
    int count = 0;
    for (KR_EventID current = m_eventQueue; current != kEndList;
         current = m_eventIndex[current].next)
    {
        if (count < capacity)
            events[count].getCopy(m_eventPool[current]);
        ++count;
    }
    return count;
}

int SimulationContext::copyEventsTo(KR_EventLabel label,
                                    KR_ObjectID destination,
                                    KR_Event *events, int capacity) const
{
    if (capacity < 0 || (capacity > 0 && events == NULL))
        return -1;
    int count = 0;
    for (KR_EventID current = m_eventQueue; current != kEndList;
         current = m_eventIndex[current].next)
    {
        const KR_Event &event = m_eventPool[current];
        if (event.label == label && event.destination == destination)
        {
            if (count < capacity)
                events[count].getCopy(event);
            ++count;
        }
    }
    return count;
}

int SimulationContext::removeEventsTo(KR_EventLabel label,
                                      KR_ObjectID destination)
{
    int count = 0;
    KR_EventID *link = &m_eventQueue;
    while (*link != kEndList)
    {
        const KR_EventID current = *link;
        if (m_eventPool[current].destination == destination &&
            m_eventPool[current].label == label)
        {
            *link = m_eventIndex[current].next;
            m_eventIndex[current].event = NULL;
            m_eventIndex[current].next = m_freeEventList;
            m_freeEventList = current;
            ++count;
        }
        else
            link = &m_eventIndex[current].next;
    }
    return count;
}

int SimulationContext::eventCount() const
{
    int count = 0;
    for (KR_EventID current = m_eventQueue; current != kEndList;
         current = m_eventIndex[current].next)
        ++count;
    return count;
}

int SimulationContext::eventFreeCount() const
{
    int count = 0;
    for (KR_EventID current = m_freeEventList; current != kEndList;
         current = m_eventIndex[current].next)
        ++count;
    return count;
}

int SimulationContext::objectFreeCount() const
{
    int count = 0;
    for (int current = m_freeObjectList; current != kEndList;
         current = m_objectIndex[current].next)
        ++count;
    return count;
}
