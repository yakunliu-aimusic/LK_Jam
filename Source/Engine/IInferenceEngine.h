#pragma once
#include <vector>
#include <juce_core/juce_core.h>

#if __has_include("../Data/EventTypes.h")
    #include "../Data/EventTypes.h"
    #include "../Data/HarmonyData.h"
#else
    #include "EventTypes.h"
    #include "HarmonyData.h"
#endif

class IInferenceEngine {
public:
    virtual ~IInferenceEngine() = default;

    virtual void processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                               std::vector<MidiEventLite>& outputPhrase,
                               const Chord& currentChord,
                               juce::int64 responseStartSample) = 0;

    virtual bool loadModel(const juce::File& modelFile) = 0;
    virtual bool isModelReady() const = 0;
};