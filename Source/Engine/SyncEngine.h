#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "StateMachine.h"

class SyncEngine {
public:
    void update(juce::AudioPlayHead* playHead, StateMachine& stateMachine, int turnBars, int cycleBars, double sampleRate, int numSamples);

    bool isStateChangedThisBlock() const { return stateChangedThisBlock; }
    bool isUsingHostClock() const       { return hostSynced; }
    double getCurrentBpm() const        { return currentBpm; }
    int getCurrentBarIndex() const      { return currentBarIndex; }
    double getCurrentPpq() const        { return currentPpq; }
    double getPpqPerBar() const         { return ppqPerBar; }

    void setSyncMode(int mode) { syncMode = mode; }
    void setManualBpm(double bpm) { manualBpm = bpm; }

    void setOverrideMode(bool override) { bIsOverridden = override; }
    bool isOverridden() const { return bIsOverridden; }

    // 🌟 对外提供的重置请求接口（统一处理时钟倒流与防死锁）
    void requestReset() { bNeedsReset = true; }

private:
    bool hostSynced = false;
    double currentBpm = 120.0;
    double currentPpq = 0.0;
    double ppqPerBar = 4.0;
    int currentBarIndex = 0;

    bool stateChangedThisBlock = false;
    InteractionState lastState = InteractionState::Idle;

    juce::int64 internalSamplesAccumulated = 0;

    int syncMode = 1;          // 1=Host, 2=Internal, 3=MIDI
    double manualBpm = 120.0;
    bool bIsOverridden = false;

    // 相对时间引擎变量
    double ppqOffset = 0.0;
    bool bNeedsReset = false;
};