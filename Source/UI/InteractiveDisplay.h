#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/PluginProcessor.h"
#include "UI_GridCanvas.h"
#include "UI_TransportBar.h"

class InteractiveDisplay : public juce::Component {
public:
    explicit InteractiveDisplay(LK_Jam_POCProcessor& p) : processor(p) {
        bIsAlive.store(true);

        roleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        roleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(roleLabel);

        toAiLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        toAiLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        toAiLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(toAiLabel);

        fromAiLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        fromAiLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff59e0b));
        fromAiLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(fromAiLabel);

        progressLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        progressLabel.setJustificationType(juce::Justification::centred);
        progressLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(progressLabel);

        addAndMakeVisible(gridCanvas);
        gridCanvas.onProgressionChanged = [this](const Progression& newProg) {
            processor.setActiveProgression(newProg);
        };

        addAndMakeVisible(transportBar);

        transportBar.onPlayToggled = [this](bool playing) {
            isPlaying = playing;
            processor.isTransportRunning.store(playing);

            if (isPlaying) {
                processor.resetLoopState();
            } else {
                processor.panicTriggered.store(true);
                gridCanvas.setPlayingIndex(-1);
            }

            repaint();
        };

        transportBar.onLockToggled = [this](bool locked) {
            gridCanvas.setLocked(locked);
        };

        transportBar.onMetronomeToggled = [this](bool enabled) {
            processor.isMetronomeEnabled.store(enabled);
        };

        // 修复：Reset 时清空进度、重置总循环次数，但不清空用户填好的和弦
        transportBar.onResetClicked = [this]() {
            processor.isTransportRunning.store(false);
            processor.resetLoopState(true);
            isPlaying = false;
            transportBar.setPlayingStateSilently(false);

            // Reset progress and total loop count without clearing user chords
            auto prog = processor.getActiveProgression();
            prog.totalLoops = 0;

            processor.setActiveProgression(prog);
            transportBar.syncSettingsFromProgression(prog); // UI 上的 Loops 文本框也会随之变成 0

            gridCanvas.setPlayingIndex(-1);
            repaint();
        };

        transportBar.onSettingsChanged = [this](int sigNum, int sigDen, int measures, int loops) {
            auto prog = processor.getActiveProgression();
            prog.timeSigNum = sigNum;
            prog.timeSigDen = sigDen;
            prog.totalLoops = loops;
            prog.measures.resize(static_cast<size_t>(measures));
            processor.setActiveProgression(prog);
            gridCanvas.loadProgression(prog);
        };

        transportBar.onPresetSelected = [this](int presetIndex) {
            auto presets = HarmonyLibrary::getPresets();
            if (presetIndex >= 0 && static_cast<size_t>(presetIndex) < presets.size()) {
                processor.setActiveProgression(presets[static_cast<size_t>(presetIndex)]);
                gridCanvas.loadProgression(presets[static_cast<size_t>(presetIndex)]);
                transportBar.syncSettingsFromProgression(presets[static_cast<size_t>(presetIndex)]);
            }
        };

        auto activeProg = processor.getActiveProgression();
        gridCanvas.loadProgression(activeProg);
        transportBar.syncSettingsFromProgression(activeProg);
    }

    ~InteractiveDisplay() override { bIsAlive.store(false); }
    bool isAlive() const { return bIsAlive.load(); }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        int stateInt = processor.getStateMachine().getStateAsInt();
        juce::Colour ledColor = juce::Colour(0xff94a3b8);
        juce::String roleText = "ROLE: IDLE";
        if (stateInt == 1) { ledColor = juce::Colour(0xff10b981); roleText = "ROLE: HUMAN"; }
        else if (stateInt == 2) { ledColor = juce::Colour(0xfff59e0b); roleText = "ROLE: AI"; }
        else if (stateInt == 3) { ledColor = juce::Colour(0xff3b82f6); roleText = "ROLE: PRE-ROLL"; }

        g.setColour(ledColor);
        g.fillEllipse(roleBounds.getX() + 16.0f, roleBounds.getY() + 18.0f, 14.0f, 14.0f);

        roleLabel.setText(roleText, juce::dontSendNotification);
        roleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));

        g.setColour(juce::Colour(0xffe2e8f0));
        g.fillRoundedRectangle(progressBarBounds, 6.0f);

        // 核心修复：进度条与 Loop 次数显示
        if (processor.isClockRunning.load()) {
            if (stateInt == 3) {
                const double beatsRemaining = processor.getSyncEngine().getPreRollBeatsRemaining();
                const double secondsRemaining = (beatsRemaining * 60.0) / std::max(1.0, processor.getSyncEngine().getCurrentBpm());
                g.setColour(juce::Colour(0xff3b82f6));
                g.fillRoundedRectangle(progressBarBounds.withWidth(progressBarBounds.getWidth()), 6.0f);
                progressLabel.setText(juce::String::formatted("Pre-Roll: %.1f sec to Loop 0", secondsRemaining), juce::dontSendNotification);
                progressLabel.setColour(juce::Label::textColourId, juce::Colours::white);
                return;
            }

            auto prog = processor.getActiveProgression();
            double ppq = processor.getCurrentRelativePpq();
            double loopBeats = std::max(1.0, prog.getTotalBeats());

            double overallProgress = 0.0;
            if (prog.totalLoops > 0) {
                double totalAbsoluteBeats = loopBeats * prog.totalLoops;
                overallProgress = juce::jlimit(0.0, 1.0, ppq / totalAbsoluteBeats);
            } else {
                overallProgress = std::fmod(ppq, loopBeats) / loopBeats;
            }

            g.setColour(juce::Colour(0xff3b82f6));
            float fillWidth = static_cast<float>(progressBarBounds.getWidth() * overallProgress);
            g.fillRoundedRectangle(progressBarBounds.withWidth(std::max(6.0f, fillWidth)), 6.0f);

            // 修复：计算当前 Loop 并严格钳制最大值
            int currentLoop = static_cast<int>(ppq / loopBeats) + 1;
            if (prog.totalLoops > 0) {
                currentLoop = std::min(currentLoop, prog.totalLoops); // 决不允许当前次数超过总次数
            }

            juce::String loopText = prog.totalLoops == 0 ? " (Infinite Loop)" : " / " + juce::String(prog.totalLoops);
            progressLabel.setText("Playing Loop " + juce::String(currentLoop) + loopText, juce::dontSendNotification);
            progressLabel.setColour(juce::Label::textColourId, overallProgress > 0.5 ? juce::Colours::white : juce::Colour(0xff0f172a));
        } else {
            progressLabel.setText("Stopped / Idle", juce::dontSendNotification);
            progressLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        }
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(8);
        auto topBar = bounds.removeFromTop(50);
        roleBounds = topBar.removeFromLeft(150).toFloat();
        roleLabel.setBounds(roleBounds.toNearestInt().withTrimmedLeft(40));

        auto dataArea = topBar.removeFromRight(200);
        dataArea.removeFromRight(60);
        toAiLabel.setBounds(dataArea.removeFromTop(25));
        fromAiLabel.setBounds(dataArea);

        bounds.removeFromTop(8);
        progressBarBounds = bounds.removeFromTop(30).toFloat().reduced(4, 4);
        progressLabel.setBounds(progressBarBounds.toNearestInt());
        bounds.removeFromTop(8);

        auto transportArea = bounds.removeFromBottom(90);
        transportBar.setBounds(transportArea);
        bounds.removeFromBottom(8);

        gridCanvas.setBounds(bounds);
    }

    void updateStateSafe() {
        if (!isAlive()) return;

        const bool transportRunning = processor.isClockRunning.load();
        if (transportRunning != isPlaying) {
            isPlaying = transportRunning;
            transportBar.setPlayingStateSilently(transportRunning);
        }

        int tCount = processor.uiToAiCount.load();
        juce::String toStr = "TO AI:  ";
        int toStart = std::max(0, tCount - 5);
        int toEnd = std::min(tCount, 16);
        for (int i = toStart; i < toEnd; ++i) {
            toStr += getNoteName(processor.uiToAiPitches[i].load()) + "  ";
        }

        int fCount = processor.uiFromAiCount.load();
        juce::String fromStr = "FROM AI:  ";
        int fromStart = std::max(0, fCount - 5);
        int fromEnd = std::min(fCount, 16);
        for (int i = fromStart; i < fromEnd; ++i) {
            fromStr += getNoteName(processor.uiFromAiPitches[i].load()) + "  ";
        }

        int activeMeasureIdx = -1;
        if (processor.isClockRunning.load()) {
            double currentPpq = processor.getCurrentRelativePpq();
            auto prog = processor.getActiveProgression();
            double accumulatedBeats = 0.0;
            double ppqInLoop = std::fmod(currentPpq, std::max(1.0, prog.getTotalBeats()));

            for (size_t i = 0; i < prog.measures.size(); ++i) {
                accumulatedBeats += prog.getBeatsPerMeasure();
                if (ppqInLoop < accumulatedBeats) {
                    activeMeasureIdx = static_cast<int>(i);
                    break;
                }
            }
        }

        bool metronomeEnabled = processor.isMetronomeEnabled.load();
        bool isFlash = false;
        float beatFraction = 0.0f;
        if (metronomeEnabled) {
            double currentPpq = processor.getCurrentRelativePpq();
            beatFraction = static_cast<float>(std::fmod(currentPpq, 1.0));
            isFlash = (beatFraction < 0.15f);
        }

        juce::MessageManager::callAsync([this, activeMeasureIdx, metronomeEnabled, isFlash, beatFraction, toStr, fromStr]() {
            if (!isAlive()) return;

            toAiLabel.setText(toStr, juce::dontSendNotification);
            fromAiLabel.setText(fromStr, juce::dontSendNotification);

            if (processor.isClockRunning.load()) gridCanvas.setPlayingIndex(activeMeasureIdx);
            else gridCanvas.setPlayingIndex(-1);

            if (metronomeEnabled) transportBar.updateMetronomeDisplay(isFlash, beatFraction);
            repaint();
        });
    }

private:
    juce::String getNoteName(int pitch) {
        if (pitch < 0 || pitch > 127) return "";
        static const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return juce::String(notes[pitch % 12]) + juce::String((pitch / 12) - 1);
    }

    std::atomic<bool> bIsAlive {false};
    LK_Jam_POCProcessor& processor;
    GridCanvas gridCanvas;
    TransportBar transportBar;

    juce::Label roleLabel, progressLabel;
    juce::Label toAiLabel, fromAiLabel;
    juce::Rectangle<float> roleBounds, progressBarBounds;
    bool isPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InteractiveDisplay)
};