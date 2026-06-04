#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/PluginProcessor.h"
#include <atomic>
#include <cmath>

class SystemStatusPanel : public juce::Component {
public:
    explicit SystemStatusPanel(LK_Jam_POCProcessor& p) : processor(p) {
        bIsAlive.store(true);

        // --- 1. RESET ALL 按钮 ---
        panicButton.setButtonText("RESET ALL (PANIC)");
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        panicButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));
        panicButton.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffcbd5e1));
        panicButton.onClick = [this]() { processor.panicTriggered.store(true); };
        addAndMakeVisible(panicButton);

        // --- 2. Test Tone 按钮 ---
        testButton.setButtonText("Test Tone 1kHz");
        testButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        testButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffcbd5e1));
        testButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));
        testButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff0f172a));
        testButton.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffcbd5e1));
        testButton.setClickingTogglesState(true);
        testButton.onClick = [this]() {
            processor.isTestToneEnabled.store(testButton.getToggleState());
        };
        addAndMakeVisible(testButton);

        // --- 3. 状态标签 ---
        statusLabel.setJustificationType(juce::Justification::centredRight);
        statusLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
        statusLabel.setText("Latency: 0.0 ms    |    CPU: 0.0 %", juce::dontSendNotification);
        addAndMakeVisible(statusLabel);
    }

    ~SystemStatusPanel() override { bIsAlive.store(false); }
    bool isAlive() const { return bIsAlive.load(); }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(16, 8);

        // 重新排版，平分空间
        panicButton.setBounds(bounds.removeFromLeft(150));
        bounds.removeFromLeft(12);

        testButton.setBounds(bounds.removeFromLeft(120));

        statusLabel.setBounds(bounds);
    }

    void updateStateSafe() {
        if (!isAlive()) return;
        updateState();
    }

private:
    void updateState() {
        const double newLatency = processor.currentLatencyMs.load();
        const double newCpu = processor.currentCpuUsage.load();

        if (std::abs(newLatency - latencyMs) > 0.1 || std::abs(newCpu - cpuUsage) > 0.5) {
            latencyMs = newLatency;
            cpuUsage = newCpu;

            juce::String text = juce::String::formatted("Latency: %.1f ms    |    CPU: %.1f %%", latencyMs, cpuUsage);
            statusLabel.setText(text, juce::dontSendNotification);

            statusLabel.setColour(juce::Label::textColourId,
                (latencyMs > 5.0 || cpuUsage > 80.0) ? juce::Colour(0xffd97706) : juce::Colour(0xff0f172a));
        }
    }

    std::atomic<bool> bIsAlive {false};
    LK_Jam_POCProcessor& processor;

    juce::TextButton panicButton, testButton;
    juce::Label statusLabel;

    double latencyMs = -1.0;
    double cpuUsage = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemStatusPanel)
};