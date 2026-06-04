#include "SessionDirector.h"

SessionDirector::SessionDirector(StateMachine& stateMachine, SyncEngine& syncEngine)
    : sm(stateMachine), sync(syncEngine) {}

void SessionDirector::process(juce::AudioProcessorValueTreeState& apvts,
                              const juce::MidiBuffer& midiMessages,
                              bool uiForceLearning)
{
    // 1. 监听外部 MIDI 触发 (例如：踏板发送 CC 64，或特定最低音 C-2)
    evaluateMidiTriggers(midiMessages);

    // 2. 监听宿主 Automation 曲线
    evaluateAutomation(apvts);

    // 3. 综合判断，覆写状态
    // 如果用户在 UI 点了 LEARNING，或者踩了踏板，强制进入 Listening（缓冲输入）
    if (uiForceLearning || isMidiTriggered || isAutomationTriggered) {
        sm.setState(InteractionState::Listening);

        // 告诉时钟引擎，不要按默认的小节循环走了，现在是手动模式
        sync.setOverrideMode(true);
    } else {
        // 如果没人干预，并且处于 Override 模式，说明刚刚放开了踏板/停止了Automation
        if (sync.isOverridden()) {
            sm.setState(InteractionState::Responding); // 踏板一松，立刻爆发响应！
            sync.setOverrideMode(false); // 恢复正常循环
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
    // 假设你在 APVTS 里加了一个叫 "aiTrigger" 的参数
    if (auto* param = apvts.getRawParameterValue("aiTrigger")) {
        float val = param->load();
        // 画线超过 0.5，视为进入强行介入模式
        isAutomationTriggered = (val > 0.5f);
    }
}