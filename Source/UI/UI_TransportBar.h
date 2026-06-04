#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../Data/HarmonyData.h"

// ==============================================================================
// 辅助传输按钮组件
// ==============================================================================
class PlayStopButton : public juce::Button {
public:
    PlayStopButton() : juce::Button("PlayStop") {}
    void setPlaying(bool isPlaying) { playing = isPlaying; repaint(); }
protected:
    void paintButton(juce::Graphics& g, bool, bool) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(playing ? juce::Colour(0xff64748b) : juce::Colour(0xff3b82f6));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colours::white);
        juce::Path icon;
        float iconX = bounds.getCentreX(), iconY = bounds.getCentreY();
        if (playing) icon.addRoundedRectangle(iconX - 6.0f, iconY - 6.0f, 12.0f, 12.0f, 2.0f);
        else icon.addTriangle(iconX - 4.0f, iconY - 7.0f, iconX - 4.0f, iconY + 7.0f, iconX + 7.0f, iconY);
        g.fillPath(icon);
    }
private: bool playing = false;
};

class LockButton : public juce::Button {
public:
    LockButton() : juce::Button("Lock") {}
    void setLocked(bool isLocked) { locked = isLocked; repaint(); }
protected:
    void paintButton(juce::Graphics& g, bool, bool) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(locked ? juce::Colour(0xffcbd5e1) : juce::Colour(0xffe2e8f0));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(locked ? juce::Colour(0xff0f172a) : juce::Colour(0xff64748b));
        float cx = bounds.getCentreX(), cy = bounds.getCentreY() + 2.0f;
        g.fillRoundedRectangle(cx - 5.0f, cy - 4.0f, 10.0f, 8.0f, 2.0f);
        juce::Path shackle;
        if (locked) shackle.addCentredArc(cx, cy - 4.0f, 3.5f, 4.0f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        else shackle.addCentredArc(cx - 2.0f, cy - 4.0f, 3.5f, 4.0f, 0.0f, -juce::MathConstants<float>::pi, 0.0f, true);
        g.strokePath(shackle, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
private: bool locked = false;
};

class MetronomeButton : public juce::Button {
public:
    MetronomeButton() : juce::Button("Metronome") { setClickingTogglesState(true); }
    void setFlashPhase(bool isFlash, float swing) { flash = isFlash; swingPhase = swing; repaint(); }
protected:
    void paintButton(juce::Graphics& g, bool, bool) override {
        auto bounds = getLocalBounds().toFloat();
        bool isOn = getToggleState();
        g.setColour(isOn ? (flash ? juce::Colour(0xfffcd34d) : juce::Colour(0xfffef3c7)) : juce::Colour(0xffe2e8f0));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(isOn ? juce::Colour(0xffd97706) : juce::Colour(0xff64748b));
        float cx = bounds.getCentreX(), cy = bounds.getCentreY() + 1.0f;
        juce::Path m;
        m.addTriangle(cx, cy - 7.0f, cx - 6.0f, cy + 7.0f, cx + 6.0f, cy + 7.0f);
        g.strokePath(m, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        float swingX = isOn ? std::sin(swingPhase * juce::MathConstants<float>::twoPi) * 5.0f : 0.0f;
        g.drawLine(cx, cy + 5.0f, cx + swingX, cy - 8.0f, 1.5f);
    }
private: bool flash = false; float swingPhase = 0.0f;
};

// ==============================================================================
// TransportBar 主组件
// ==============================================================================
class TransportBar : public juce::Component {
public:
    std::function<void(bool)> onPlayToggled;
    std::function<void(bool)> onLockToggled;
    std::function<void(bool)> onMetronomeToggled;
    std::function<void(int sigNum, int sigDen, int measures, int loops)> onSettingsChanged;
    std::function<void(int)> onPresetSelected;
    std::function<void()> onResetClicked;

    TransportBar() {
        // --- 第一排设置项 ---
        timeSigCombo.addItemList({"4/4", "3/4", "6/8"}, 1);
        timeSigCombo.setSelectedId(1, juce::dontSendNotification);
        timeSigCombo.onChange = [this] { triggerSettingsChange(); };
        addAndMakeVisible(timeSigCombo);

        measureInput.setText("16", juce::dontSendNotification);
        measureInput.setEditable(true);
        measureInput.setJustificationType(juce::Justification::centred);
        measureInput.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
        measureInput.setColour(juce::Label::textWhenEditingColourId, juce::Colour(0xff0f172a));
        measureInput.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xfff1f5f9));
        measureInput.onTextChange = [this] { triggerSettingsChange(); };
        addAndMakeVisible(measureInput);

        loopInput.setText("0", juce::dontSendNotification);
        loopInput.setEditable(true);
        loopInput.setJustificationType(juce::Justification::centred);
        loopInput.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
        loopInput.setColour(juce::Label::textWhenEditingColourId, juce::Colour(0xff0f172a));
        loopInput.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xfff1f5f9));
        loopInput.onTextChange = [this] { triggerSettingsChange(); };
        addAndMakeVisible(loopInput);

        presetCombo.addItemList({"Templates...", "Blank (16 Bars)", "Jazz Blues (12 Bars)"}, 1);
        presetCombo.setSelectedId(1, juce::dontSendNotification);
        presetCombo.onChange = [this] {
            int idx = presetCombo.getSelectedItemIndex() - 1;
            if (idx >= 0 && onPresetSelected) onPresetSelected(idx);
            presetCombo.setSelectedId(1, juce::dontSendNotification);
        };
        addAndMakeVisible(presetCombo);

        // --- 第二排控制组件 ---
        playBtn.onClick = [this] {
            isPlaying = !isPlaying;
            playBtn.setPlaying(isPlaying);
            if (onPlayToggled) onPlayToggled(isPlaying);
        };
        addAndMakeVisible(playBtn);

        lockBtn.onClick = [this] {
            isLocked = !isLocked;
            lockBtn.setLocked(isLocked);

            timeSigCombo.setEnabled(!isLocked);
            measureInput.setEditable(!isLocked);
            loopInput.setEditable(!isLocked);
            presetCombo.setEnabled(!isLocked);
            resetBtn.setEnabled(!isLocked);

            if (onLockToggled) onLockToggled(isLocked);
        };
        addAndMakeVisible(lockBtn);

        metronomeBtn.onClick = [this] { if (onMetronomeToggled) onMetronomeToggled(metronomeBtn.getToggleState()); };
        addAndMakeVisible(metronomeBtn);

        // 初始化 Reset 按钮
        resetBtn.setButtonText("RESET");
        resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfffee2e2));
        resetBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffef4444));
        resetBtn.onClick = [this] { if (onResetClicked) onResetClicked(); };
        addAndMakeVisible(resetBtn);

        addAndMakeVisible(mLabel); mLabel.setText("Bars:", juce::dontSendNotification);
        addAndMakeVisible(lLabel); lLabel.setText("Loops:", juce::dontSendNotification);
    }

    void syncSettingsFromProgression(const Progression& prog) {
        if (prog.timeSigNum == 4 && prog.timeSigDen == 4) timeSigCombo.setSelectedId(1, juce::dontSendNotification);
        else if (prog.timeSigNum == 3 && prog.timeSigDen == 4) timeSigCombo.setSelectedId(2, juce::dontSendNotification);
        else if (prog.timeSigNum == 6 && prog.timeSigDen == 8) timeSigCombo.setSelectedId(3, juce::dontSendNotification);

        measureInput.setText(juce::String(prog.measures.size()), juce::dontSendNotification);
        loopInput.setText(juce::String(prog.totalLoops), juce::dontSendNotification);
    }

    void setPlayingStateSilently(bool playing) {
        isPlaying = playing;
        playBtn.setPlaying(playing);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(8);
        auto row1 = bounds.removeFromTop(30);
        bounds.removeFromTop(10);
        auto row2 = bounds.removeFromTop(32);

        // 第一排基础参数
        timeSigCombo.setBounds(row1.removeFromLeft(70));
        row1.removeFromLeft(12);
        mLabel.setBounds(row1.removeFromLeft(35));
        measureInput.setBounds(row1.removeFromLeft(35));
        row1.removeFromLeft(12);
        lLabel.setBounds(row1.removeFromLeft(42));
        loopInput.setBounds(row1.removeFromLeft(35));

        presetCombo.setBounds(row1.removeFromRight(150));

        // 🌟 第二排控制区：扩大横向居中容器，将 Reset 按钮精准塞到 Play 的右手边
        auto centerControl = row2.withSizeKeepingCentre(235, 32);
        metronomeBtn.setBounds(centerControl.removeFromLeft(40));
        centerControl.removeFromLeft(15);
        lockBtn.setBounds(centerControl.removeFromLeft(40));
        centerControl.removeFromLeft(15);
        playBtn.setBounds(centerControl.removeFromLeft(40));
        centerControl.removeFromLeft(15);

        // Reset 按钮就位
        resetBtn.setBounds(centerControl.removeFromLeft(70));
    }

    // 修复：添加 public 方法，解决找不到函数的错误
    void updateMetronomeDisplay(bool isFlash, float swing) {
        metronomeBtn.setFlashPhase(isFlash, swing);
    }

private:
    void triggerSettingsChange() {
        int sigNum = 4, sigDen = 4;
        if (timeSigCombo.getSelectedId() == 2) { sigNum = 3; }
        else if (timeSigCombo.getSelectedId() == 3) { sigNum = 6; sigDen = 8; }

        int measures = std::max(1, measureInput.getText().getIntValue());
        int loops = std::max(0, loopInput.getText().getIntValue());

        if (onSettingsChanged) onSettingsChanged(sigNum, sigDen, measures, loops);
    }

    PlayStopButton playBtn;
    LockButton lockBtn;
    MetronomeButton metronomeBtn;
    juce::TextButton resetBtn;

    juce::ComboBox timeSigCombo, presetCombo;
    juce::Label measureInput, loopInput, mLabel, lLabel;

    bool isPlaying = false, isLocked = false;
};