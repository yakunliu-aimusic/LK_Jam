#pragma once

#include "StateMachine.h"
#include "SyncEngine.h"
#include "../Data/EventTypes.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

class SessionDirector {
public:
    SessionDirector(StateMachine& stateMachine, SyncEngine& syncEngine);

    // 每帧调用，决定当前到底该处于什么状态
    void process(juce::AudioProcessorValueTreeState& apvts,
                 const juce::MidiBuffer& midiMessages,
                 bool uiForceLearning);

private:
    void evaluateAutomation(juce::AudioProcessorValueTreeState& apvts);
    void evaluateMidiTriggers(const juce::MidiBuffer& midiMessages);

    StateMachine& sm;
    SyncEngine& sync;

    // 触发权重：MIDI 踩踏板 > UI 强制点击 > Automation 曲线 > 默认时钟循环
    bool isMidiTriggered = false;
    bool isAutomationTriggered = false;
};