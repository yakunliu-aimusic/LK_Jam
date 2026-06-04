#pragma once
#include <juce_core/juce_core.h>

// 交互状态枚举
enum class InteractionState {
    Idle = 0,       // 空闲状态（宿主停止）
    Listening = 1,  // 监听状态（录制用户输入）
    Responding = 2  // 响应状态（AI生成输出）
};

class StateMachine {
public:
    StateMachine() : currentState(InteractionState::Idle) {}

    void setState(InteractionState newState) {
        if (newState != currentState) {
            currentState = newState;
            DBG("📊 State changed to: " << getStateAsString());
        }
    }

    InteractionState getState() const { return currentState; }
    int getStateAsInt() const { return static_cast<int>(currentState); }

private:
    InteractionState currentState;

    const char* getStateAsString() const {
        switch (currentState) {
            case InteractionState::Idle: return "Idle";
            case InteractionState::Listening: return "Listening";
            case InteractionState::Responding: return "Responding";
            default: return "Unknown";
        }
    }
};