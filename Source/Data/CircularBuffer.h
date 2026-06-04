#pragma once
#include <vector>
#include "EventTypes.h"

class CircularBuffer {
public:
    CircularBuffer() {
        events.reserve(4096);
    }

    void clear() { events.clear(); }

    void addEvent(const MidiEventLite& event) {
        if (events.size() < 4096) {
            events.push_back(event);
        }
    }

    const std::vector<MidiEventLite>& getRecordedEvents() const {
        return events;
    }

private:
    std::vector<MidiEventLite> events;
};