#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "StateMachine.h"

class SyncEngine {
public:
    void update(juce::AudioPlayHead* playHead, StateMachine& stateMachine, int turnBars, int cycleBars, double sampleRate, int numSamples, bool transportRunning);

    bool isStateChangedThisBlock() const { return stateChangedThisBlock; }
    bool isUsingHostClock() const       { return hostSynced; }
    double getCurrentBpm() const        { return currentBpm; }
    int getCurrentBarIndex() const      { return currentBarIndex; }
    double getCurrentPpq() const        { return currentPpq; }
    double getPpqPerBar() const         { return ppqPerBar; }
    double getPreRollBeatsRemaining() const { return preRollBeatsRemaining; }

    int getSyncMode() const             { return syncMode; }
    void setSyncMode(int mode) { syncMode = mode; }
    void setManualBpm(double bpm) { manualBpm = bpm; }

    void setOverrideMode(bool override) { bIsOverridden = override; }
    bool isOverridden() const { return bIsOverridden; }

    void armPreRoll(double rawPpq, double currentPpqPerBar, double bpm);
    void disarmPreRoll();
    bool isPreRollArmed() const { return preRollArmed; }
    bool hasFormalStart() const { return formalStarted; }

    // Anchor current host PPQ as plugin Loop 0 immediately.
    void anchorToHostPpq(double rawPpq, double currentPpqPerBar);

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

    bool preRollArmed = false;
    bool formalStarted = false;
    double preRollStartRawPpq = 0.0;
    double formalStartRawPpq = 0.0;
    double preRollBeatsRemaining = 0.0;
};