#include "SyncEngine.h"

void SyncEngine::update(juce::AudioPlayHead* playHead, StateMachine& stateMachine, int turnBars, int cycleBars, double sampleRate, int numSamples) {
    // 消除未使用的参数警告
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
            double samplesPerBeat = (sampleRate * 60.0) / currentBpm;
            internalSamplesAccumulated += numSamples;
            rawPpq = internalSamplesAccumulated / samplesPerBeat;
        }
    } else if (syncMode == 2) {  // Internal Clock
        hostSynced = false;
        currentBpm = manualBpm;
        double samplesPerBeat = (sampleRate * 60.0) / currentBpm;
        internalSamplesAccumulated += numSamples;
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
        bNeedsReset = false;
    }

    // --- 3. 计算相对时间 ---
    currentPpq = rawPpq - ppqOffset;
    if (currentPpq < 0.0) currentPpq = 0.0;

    // --- 4. 纯粹的 Trading Fours 回合制交替逻辑 ---
    currentBarIndex = static_cast<int>(currentPpq / std::max(1.0, ppqPerBar));

    if (!bIsOverridden) {
        int safeTurnBars = std::max(1, turnBars);

        // 计算目前处于绝对的第几个回合 (0, 1, 2, 3...)
        int currentTurnCount = currentBarIndex / safeTurnBars;

        // 偶数回合归人类，奇数回合归AI，和和弦长度完全脱钩！
        InteractionState newState = (currentTurnCount % 2 == 0) ? InteractionState::Listening : InteractionState::Responding;

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