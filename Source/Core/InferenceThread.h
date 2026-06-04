#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <vector>

#if __has_include("../Data/LockFreeQueue.h")
    #include "../Data/LockFreeQueue.h"
    #include "../Data/EventTypes.h"
    #include "../Data/HarmonyData.h"
#else
    #include "LockFreeQueue.h"
    #include "EventTypes.h"
    #include "HarmonyData.h"
#endif

#include "../Engine/IInferenceEngine.h"

class InferenceThread : public juce::Thread {
public:
    InferenceThread(LockFreeQueue<MidiEventLite>& outQueue);
    ~InferenceThread() override;

    void setEngine(std::unique_ptr<IInferenceEngine> engine);
    void clearQueues();
    void submitInputPhrase(const std::vector<MidiEventLite>& input, const Chord& chord, juce::int64 startSample);

    bool loadModel(const juce::File& file) {
        if (inferenceEngine != nullptr) {
            return inferenceEngine->loadModel(file);
        }
        return false;
    }

    void run() override;

private:
    LockFreeQueue<MidiEventLite>& playbackQueue;
    LockFreeQueue<MidiEventLite> inputQueue;
    std::unique_ptr<IInferenceEngine> inferenceEngine;

    std::vector<MidiEventLite> localInputBuffer;
    std::vector<MidiEventLite> localOutputBuffer;

    Chord currentChordCtx;
    juce::int64 currentStartSample = 0;

    juce::WaitableEvent triggerEvent;
    std::atomic<bool> isProcessing{false};
};