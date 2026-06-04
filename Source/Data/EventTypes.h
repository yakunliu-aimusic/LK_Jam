#pragma once

struct MidiEventLite {
    int sampleOffset;
    int pitch;
    int velocity;
    bool isNoteOn;
};

// 如果你有定义 EventRole (区分人类和AI)，可以补在这里
// enum class EventRole { Human, AI };