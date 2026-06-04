#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "../Data/CircularBuffer.h"
#include "../Data/LockFreeQueue.h"
#include "../Engine/StateMachine.h"
#include "../Engine/SyncEngine.h"
#include "../Engine/SessionDirector.h"
#include "InferenceThread.h"
#include "../Data/HarmonyData.h"

class LK_Jam_POCProcessor : public juce::AudioProcessor {
public:
    LK_Jam_POCProcessor();
    ~LK_Jam_POCProcessor() override;

    void loadCustomModel(const juce::File& file) { inferenceThread.loadModel(file); }

    // 供 UI 线程调用 (写入并原子切换)
    void setActiveProgression(const Progression& prog) {
        int nextIdx = (activeProgressionIndex.load(std::memory_order_relaxed) + 1) % 2;
        progressionBuffers[static_cast<size_t>(nextIdx)] = prog;
        activeProgressionIndex.store(nextIdx, std::memory_order_release);
    }

    // 供音频线程调用 (只读)
    const Progression& getActiveProgression() const {
        return progressionBuffers[static_cast<size_t>(activeProgressionIndex.load(std::memory_order_acquire))];
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LK_Jam"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { (void)index; }
    const juce::String getProgramName(int index) override { (void)index; return {}; }
    void changeProgramName(int index, const juce::String& newName) override {
        (void)index;
        (void)newName;
    }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    StateMachine getStateMachine() const { return stateMachine; }
    SyncEngine& getSyncEngine() { return syncEngine; }

    std::atomic<bool> panicTriggered{false};
    std::atomic<bool> isHostSynced{false};
    std::atomic<double> currentBpm{120.0};
    std::atomic<double> currentLatencyMs{0.0};
    std::atomic<double> currentCpuUsage{0.0};

    std::atomic<bool> isAiPlaying{true};
    std::atomic<bool> isTestToneEnabled{false};

    std::atomic<bool> isLearning{false};

    std::atomic<int> uiToAiPitches[16];
    std::atomic<int> uiToAiCount{0};

    std::atomic<int> uiFromAiPitches[16];
    std::atomic<int> uiFromAiCount{0};

    std::atomic<bool> isMetronomeEnabled{false};
    float metronomePhase = 0.0f;
    float metronomeEnvelope = 0.0f;
    int lastMetronomeBeat = -1;
    void resetLoopState();

    // 🌟 修复：完美消除 1 帧视觉闪烁
    double getCurrentRelativePpq() const {
        if (shouldResetClock.load()) {
            return 0.0;
        }
        return syncEngine.getCurrentPpq();
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    StateMachine stateMachine;
    SyncEngine syncEngine;

    SessionDirector sessionDirector;

    std::array<Progression, 2> progressionBuffers;
    std::atomic<int> activeProgressionIndex { 0 };

    CircularBuffer captureBuffer;
    LockFreeQueue<MidiEventLite> playbackQueue;

    InferenceThread inferenceThread;

    std::array<bool, 128> activeNotes {false};

    std::atomic<float>* turnBarsParam = nullptr;
    std::atomic<float>* cycleBarsParam = nullptr;
    std::atomic<float>* modelChoiceParam = nullptr;
    std::atomic<float>* fallbackModeParam = nullptr;

    std::optional<MidiEventLite> cachedNextEvent;

    juce::int64 currentAbsoluteSample = 0;

    double currentSampleRate = 44100.0;
    float testTonePhase = 0.0f;

    std::atomic<bool> shouldResetClock { false };
    std::atomic<double> ppqOffset { 0.0 };
    std::array<bool, 128> fallbackActiveNotes { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LK_Jam_POCProcessor)
};