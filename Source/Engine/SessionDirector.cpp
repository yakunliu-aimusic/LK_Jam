#include "SessionDirector.h"

SessionDirector::SessionDirector(StateMachine& stateMachine, SyncEngine& syncEngine)
    : sm(stateMachine), sync(syncEngine) {}

void SessionDirector::process(juce::AudioProcessorValueTreeState& apvts,
                              const juce::MidiBuffer& midiMessages,
                              bool uiForceLearning)
{
    if (sync.getSyncMode() == 1) {
        return;
    }

    evaluateMidiTriggers(midiMessages);
    evaluateAutomation(apvts);

    if (uiForceLearning || isMidiTriggered || isAutomationTriggered) {
        sm.setState(InteractionState::Listening);
        sync.setOverrideMode(true);
    } else {
        if (sync.isOverridden()) {
            sm.setState(InteractionState::Responding);
            sync.setOverrideMode(false);
        }
    }
}

void SessionDirector::evaluateMidiTriggers(const juce::MidiBuffer& midiMessages) {
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        // 假设外部脚踏板发送 CC 64 (延音踏板) 作为 AI 触发器
        if (msg.isController() && msg.getControllerNumber() == 64) {
            isMidiTriggered = (msg.getControllerValue() > 64);
        }
        // 或者：按下了极低音 C-2 (MIDI 0) 开始监听，松开爆发
        else if (msg.isNoteOn() && msg.getNoteNumber() == 0) {
            isMidiTriggered = true;
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == 0) {
            isMidiTriggered = false;
        }
    }
}

void SessionDirector::evaluateAutomation(juce::AudioProcessorValueTreeState& apvts) {
    if (sync.getSyncMode() == 1) {
        isAutomationTriggered = false;
        return;
    }

    if (auto* param = apvts.getRawParameterValue("aiTrigger")) {
        float val = param->load();
        isAutomationTriggered = (val > 0.5f);
    }
}
