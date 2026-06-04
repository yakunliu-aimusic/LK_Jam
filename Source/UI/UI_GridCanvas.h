#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/HarmonyData.h"
#include <vector>
#include <functional>

class MeasureBox : public juce::Component {
public:
    std::function<void(int, const Chord&)> onChordValidated;
    std::function<void(int)> onFocusGained;

    MeasureBox(int idx) : index(idx) {
        inputLabel.setJustificationType(juce::Justification::centred);
        inputLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        inputLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentWhite);
        inputLabel.setColour(juce::Label::outlineColourId, juce::Colours::transparentWhite);
        inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
        inputLabel.setColour(juce::Label::textWhenEditingColourId, juce::Colour(0xff0f172a));

        inputLabel.onEditorShow = [this] { if (onFocusGained) onFocusGained(index); };

        // 🌟 完全保留你的核心逻辑：用户输入模式 + 合法性校验
        inputLabel.onTextChange = [this] {
            juce::String text = inputLabel.getText().trim();
            if (text.isEmpty()) {
                currentChord = Chord();
                inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
                if (onChordValidated) onChordValidated(index, currentChord);
                return;
            }

            Chord parsed = Chord::fromString(text);
            if (parsed.isEmpty()) {
                // ❌ 输入不合法：文字变红，拒绝向底层发送数据
                inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
            } else {
                // ✅ 输入合法：恢复颜色，格式化文本，并同步给底层
                currentChord = parsed;
                inputLabel.setText(parsed.name, juce::dontSendNotification);
                inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
                if (onChordValidated) onChordValidated(index, currentChord);
            }
        };
        addAndMakeVisible(inputLabel);
    }

    void setChord(const Chord& c) {
        currentChord = c;
        inputLabel.setText(c.name, juce::dontSendNotification);
        inputLabel.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
    }

    // 控制是否允许用户点击编辑
    void setLocked(bool locked) { inputLabel.setEditable(!locked); }
    void setActive(bool active) { isActive = active; repaint(); }
    void setPlaying(bool playing) { isPlaying = playing; repaint(); }
    void resized() override { inputLabel.setBounds(getLocalBounds()); }

protected:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        if (isPlaying) g.setColour(juce::Colour(0xfffcd34d));
        else if (isActive) g.setColour(juce::Colour(0xfffef08a));
        else g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isActive ? juce::Colour(0xffd97706) : juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 4.0f, isActive ? 2.0f : 1.0f);

        g.setColour(juce::Colour(0xff94a3b8));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(juce::String(index + 1), bounds.reduced(4, 2), juce::Justification::topLeft);
    }

private:
    int index;
    Chord currentChord;
    juce::Label inputLabel;
    bool isActive = false;
    bool isPlaying = false;
};

class GridCanvas : public juce::Component {
public:
    std::function<void(const Progression&)> onProgressionChanged;

    GridCanvas() {}

    void loadProgression(const Progression& prog) {
        currentProg = prog;
        size_t numMeasures = prog.measures.size();
        boxes.clear();

        for (size_t i = 0; i < numMeasures; ++i) {
            auto box = std::make_unique<MeasureBox>(static_cast<int>(i));
            box->setChord(prog.measures[i].chord);
            box->setLocked(isLocked);

            box->onFocusGained = [this](int idx) {
                cursorIndex = idx;
                for (size_t j = 0; j < boxes.size(); ++j) {
                    boxes[j]->setActive(j == static_cast<size_t>(cursorIndex));
                }
            };

            // 当 MeasureBox 校验通过时，更新数组并发送给底层
            box->onChordValidated = [this](int idx, const Chord& c) {
                currentProg.measures[static_cast<size_t>(idx)].chord = c;
                if (onProgressionChanged) onProgressionChanged(currentProg);
            };

            addAndMakeVisible(*box);
            boxes.push_back(std::move(box));
        }
        resized();
    }

    void setLocked(bool locked) {
        isLocked = locked;
        for (auto& box : boxes) box->setLocked(locked);
        if (locked) {
            cursorIndex = -1;
            for (auto& box : boxes) box->setActive(false);
        }
    }

    void setPlayingIndex(int idx) {
        for (size_t i = 0; i < boxes.size(); ++i) {
            boxes[i]->setPlaying(i == static_cast<size_t>(idx));
        }
    }

protected:
    void paint(juce::Graphics& g) override {
        g.setColour(juce::Colour(0xfff8fafc));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(8);
        if (boxes.empty()) return;

        const int cols = 4;
        const int rows = (static_cast<int>(boxes.size()) + cols - 1) / cols;
        const int spacing = 6;

        int boxWidth = (bounds.getWidth() - spacing * (cols - 1)) / cols;
        int boxHeight = (bounds.getHeight() - spacing * (rows - 1)) / rows;

        for (size_t i = 0; i < boxes.size(); ++i) {
            int row = static_cast<int>(i) / cols;
            int col = static_cast<int>(i) % cols;
            boxes[i]->setBounds(bounds.getX() + col * (boxWidth + spacing),
                                bounds.getY() + row * (boxHeight + spacing),
                                boxWidth, boxHeight);
        }
    }

private:
    std::vector<std::unique_ptr<MeasureBox>> boxes;
    Progression currentProg;
    int cursorIndex = -1;
    bool isLocked = false;
};