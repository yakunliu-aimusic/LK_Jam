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
    void submitInputPhrase(const std::vector<MidiEventLite>& input,
                           const Chord& chord,
                           juce::int64 startSample,
                           juce::int64 responseLengthSamples,
                           double sampleRate,
                           double bpm);

    void submitFallbackPhrase(const std::vector<MidiEventLite>& input,
                              const Chord& chord,
                              juce::int64 startSample,
                              juce::int64 responseLengthSamples,
                              double sampleRate,
                              double bpm);

    bool loadModel(const juce::File& file) {
        if (inferenceEngine != nullptr) {
            return inferenceEngine->loadModel(file);
        }
        return false;
    }

    void resetModelMemory() {
        if (inferenceEngine != nullptr) {
            inferenceEngine->resetModelMemory();
        }
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
    juce::int64 currentResponseLengthSamples = 0;
    double currentSampleRate = 44100.0;
    double currentBpm = 120.0;

    enum class TaskMode { Model, Fallback };
    TaskMode currentTaskMode = TaskMode::Model;

    juce::WaitableEvent triggerEvent;
    std::atomic<bool> isProcessing{false};
};