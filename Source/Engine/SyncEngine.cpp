#include "SyncEngine.h"
#include <cmath>

void SyncEngine::armPreRoll(double rawPpq, double currentPpqPerBar, double bpm) {
    ppqPerBar = std::max(1.0, currentPpqPerBar);
    currentBpm = std::max(1.0, bpm);
    const double fixedPreRollSeconds = 20.0;
    const double preRollBeats = (currentBpm / 60.0) * fixedPreRollSeconds;
    formalStartRawPpq = rawPpq + preRollBeats;

    preRollStartRawPpq = rawPpq;
    preRollBeatsRemaining = std::max(0.0, formalStartRawPpq - rawPpq);
    preRollArmed = true;
    formalStarted = false;
    bIsOverridden = false;
}

void SyncEngine::disarmPreRoll() {
    preRollArmed = false;
    formalStarted = false;
    preRollStartRawPpq = 0.0;
    formalStartRawPpq = 0.0;
    preRollBeatsRemaining = 0.0;
}

void SyncEngine::anchorToHostPpq(double rawPpq, double currentPpqPerBar) {
    ppqPerBar = std::max(1.0, currentPpqPerBar);
    ppqOffset = rawPpq;
    currentPpq = 0.0;
    currentBarIndex = 0;
    internalSamplesAccumulated = 0;
    preRollArmed = false;
    formalStarted = false;
    preRollStartRawPpq = 0.0;
    formalStartRawPpq = 0.0;
    preRollBeatsRemaining = 0.0;
    bNeedsReset = false;
    bIsOverridden = false;
    lastState = InteractionState::Idle;
    stateChangedThisBlock = false;
}

void SyncEngine::update(juce::AudioPlayHead* playHead, StateMachine& stateMachine, int turnBars, int cycleBars, double sampleRate, int numSamples, bool transportRunning) {
    juce::ignoreUnused(cycleBars);
    stateChangedThisBlock = false;
    double rawPpq = 0.0;
    ppqPerBar = 4.0;

    // --- 1. 获取系统绝对时间 ---
    if (syncMode == 1) {  // Host Sync
        if (playHead != nullptr && playHead->getPosition() && playHead->getPosition()->getIsPlaying()) {
            hostSynced = true;
            auto posInfo = playHead->getPosition();
            currentBpm = posInfo->getBpm().orFallback(120.0);
            rawPpq = posInfo->getPpqPosition().orFallback(0.0);
            auto sig = posInfo->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{4, 4});
            ppqPerBar = sig.numerator * (4.0 / sig.denominator);
        } else {
            hostSynced = false;
            currentBpm = manualBpm;
            rawPpq = internalSamplesAccumulated / ((sampleRate * 60.0) / currentBpm);
        }
    } else if (syncMode == 2) {  // Internal Clock
        hostSynced = false;
        currentBpm = manualBpm;
        double samplesPerBeat = (sampleRate * 60.0) / currentBpm;
        if (transportRunning) {
            internalSamplesAccumulated += numSamples;
        }
        rawPpq = internalSamplesAccumulated / samplesPerBeat;
    } else {  // MIDI Clock In
        hostSynced = true;
        currentBpm = manualBpm;
        rawPpq = 0.0;
    }

    // --- 2. 处理重置与死锁打破 ---
    if (bNeedsReset) {
        internalSamplesAccumulated = 0;
        if (hostSynced) {
            ppqOffset = rawPpq; // 锚定当前绝对时间
        } else {
            ppqOffset = 0.0;
            rawPpq = 0.0;
        }

        // 🌟 强行清空历史状态记忆，逼迫底层状态机在下一帧唤醒
        lastState = InteractionState::Idle;
        formalStarted = false;
        preRollArmed = false;
        bNeedsReset = false;
    }

    // --- 3. 计算相对时间；PreRoll 阶段不计入正式 loop ---
    if (preRollArmed && !formalStarted) {
        if (!transportRunning) {
            disarmPreRoll();
        } else if (rawPpq < formalStartRawPpq) {
            preRollBeatsRemaining = std::max(0.0, formalStartRawPpq - rawPpq);
            currentPpq = 0.0;
            currentBarIndex = 0;
            if (lastState != InteractionState::PreRoll) {
                stateMachine.setState(InteractionState::PreRoll);
                stateChangedThisBlock = true;
                lastState = InteractionState::PreRoll;
            }
            return;
        } else {
            ppqOffset = formalStartRawPpq;
            currentPpq = rawPpq - ppqOffset;
            preRollArmed = false;
            formalStarted = true;
            preRollBeatsRemaining = 0.0;
            lastState = InteractionState::Idle;
        }
    } else {
        currentPpq = rawPpq - ppqOffset;
    }

    if (currentPpq < 0.0) currentPpq = 0.0;

    // --- 4. 整 Loop 角色切换：Loop 0 Human，Loop 1 AI，Loop 2 Human，Loop 3 AI ---
    currentBarIndex = static_cast<int>(currentPpq / std::max(1.0, ppqPerBar));

    if (!transportRunning) {
        if (lastState != InteractionState::Idle) {
            stateMachine.setState(InteractionState::Idle);
            stateChangedThisBlock = true;
            lastState = InteractionState::Idle;
        }
        return;
    }

    if (!bIsOverridden) {
        int safeTurnBars = std::max(1, turnBars);
        int loopIndex = currentBarIndex / safeTurnBars;

        InteractionState newState = (loopIndex % 2 == 0) ? InteractionState::Listening : InteractionState::Responding;

        if (newState != lastState) {
            stateMachine.setState(newState);
            stateChangedThisBlock = true;
            lastState = newState;
        }
    } else {
        InteractionState currentState = stateMachine.getState();
        if (currentState != lastState) {
            stateChangedThisBlock = true;
            lastState = currentState;
        } else {
            stateChangedThisBlock = false;
        }
    }
}