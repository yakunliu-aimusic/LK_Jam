#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/HarmonyData.h"
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>

class MeasureBox : public juce::Component {
public:
    std::function<void(int measureIndex, int beatIndex, const Chord&)> onBeatChordValidated;
    std::function<void(int measureIndex, int beatIndex)> onFocusGained;

    MeasureBox(int idx, int beats) : index(idx), beatsPerMeasure(std::max(1, beats)) {
        rebuildBeatLabels();
    }

    void setMeasure(const Measure& measure, int beats) {
        beatsPerMeasure = std::max(1, beats);
        currentMeasure = measure;
        currentMeasure.ensureBeatChords(beatsPerMeasure);
        rebuildBeatLabels();
        syncLabelsFromMeasure();
    }

    void setLocked(bool locked) {
        isLocked = locked;
        for (auto& label : beatLabels) {
            label->setEditable(!locked);
        }
    }

    void setActiveBeat(int beatIndex) {
        activeBeatIndex = beatIndex;
        repaint();
    }

    void clearActiveBeat() {
        activeBeatIndex = -1;
        repaint();
    }

    void setPlaying(bool playing) { isPlaying = playing; repaint(); }

    void resized() override {
        auto bounds = getLocalBounds().reduced(6);
        auto header = bounds.removeFromTop(14);
        juce::ignoreUnused(header);

        if (beatLabels.empty()) return;

        for (auto& label : beatLabels) {
            label->setVisible(isExpanded);
        }

        if (!isExpanded) return;

        const int spacing = 3;
        const int labelWidth = std::max(18, (bounds.getWidth() - spacing * (beatsPerMeasure - 1)) / beatsPerMeasure);
        for (int i = 0; i < static_cast<int>(beatLabels.size()); ++i) {
            beatLabels[static_cast<size_t>(i)]->setBounds(bounds.removeFromLeft(labelWidth).reduced(0, 1));
            bounds.removeFromLeft(spacing);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const int beat = beatIndexFromPosition(e.position.x);
        focusBeat(beat);

        if (e.getNumberOfClicks() > 1 && !isLocked) {
            if (!isExpanded) {
                isExpanded = true;
                resized();
                repaint();
                return;
            }

            if (isValidBeat(beat)) {
                beatLabels[static_cast<size_t>(beat)]->showEditor();
            }
        }
    }

protected:
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        if (isPlaying) g.setColour(juce::Colour(0xfffff1c2));
        else g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText("M" + juce::String(index + 1), getLocalBounds().reduced(8, 2).removeFromTop(14), juce::Justification::centredLeft);

        if (isExpanded) {
            if (beatLabels.size() > 1) {
                g.setColour(juce::Colour(0xffeef2f7));
                for (size_t i = 1; i < beatLabels.size(); ++i) {
                    auto labelBounds = beatLabels[i]->getBounds().toFloat();
                    const float x = labelBounds.getX() - 1.5f;
                    g.drawLine(x, bounds.getY() + 22.0f, x, bounds.getBottom() - 8.0f, 1.0f);
                }
            }

            if (isValidBeat(activeBeatIndex)) {
                auto labelBounds = beatLabels[static_cast<size_t>(activeBeatIndex)]->getBounds().toFloat().expanded(2.0f);
                g.setColour(juce::Colour(0xfff59e0b));
                g.drawRoundedRectangle(labelBounds, 4.0f, 2.0f);
            }
            return;
        }

        auto spanArea = getLocalBounds().reduced(8, 6).withTrimmedTop(16).toFloat();
        if (spanArea.getWidth() <= 0.0f || spanArea.getHeight() <= 0.0f) return;

        int beat = 0;
        while (beat < beatsPerMeasure) {
            auto chord = currentMeasure.getChordForBeat(beat, beatsPerMeasure);
            int spanLength = 1;
            while (beat + spanLength < beatsPerMeasure) {
                auto nextChord = currentMeasure.getChordForBeat(beat + spanLength, beatsPerMeasure);
                if (nextChord.name != chord.name || nextChord.quality != chord.quality || nextChord.rootMidi != chord.rootMidi) break;
                ++spanLength;
            }

            const float startRatio = static_cast<float>(beat) / static_cast<float>(beatsPerMeasure);
            const float widthRatio = static_cast<float>(spanLength) / static_cast<float>(beatsPerMeasure);
            auto spanBounds = juce::Rectangle<float>(spanArea.getX() + spanArea.getWidth() * startRatio,
                                                    spanArea.getY(),
                                                    spanArea.getWidth() * widthRatio,
                                                    spanArea.getHeight()).reduced(1.0f, 0.0f);

            g.setColour(chord.isEmpty() ? juce::Colour(0xfff8fafc) : juce::Colour(0xfffef3c7));
            g.fillRoundedRectangle(spanBounds, 4.0f);
            g.setColour(chord.isEmpty() ? juce::Colour(0xffe2e8f0) : juce::Colour(0xfffde68a));
            g.drawRoundedRectangle(spanBounds, 4.0f, 1.0f);

            g.setColour(chord.isEmpty() ? juce::Colour(0xff94a3b8) : juce::Colour(0xff0f172a));
            const float spanWidth = spanBounds.getWidth();
            const float fontSize = spanWidth < 34.0f ? 8.5f : (spanWidth < 48.0f ? 9.5f : (spanLength == 1 ? 10.5f : 13.0f));
            g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
            g.drawFittedText(chord.isEmpty() ? "-" : chord.name,
                             spanBounds.reduced(2.0f, 0.0f).toNearestInt(),
                             juce::Justification::centred,
                             1,
                             0.55f);

            if (isValidBeat(activeBeatIndex) && activeBeatIndex >= beat && activeBeatIndex < beat + spanLength) {
                g.setColour(juce::Colour(0xfff59e0b));
                g.drawRoundedRectangle(spanBounds.expanded(1.0f), 4.0f, 1.5f);
            }

            beat += spanLength;
        }
    }

private:
    void rebuildBeatLabels() {
        beatLabels.clear();
        for (int beat = 0; beat < beatsPerMeasure; ++beat) {
            auto label = std::make_unique<juce::Label>();
            label->setJustificationType(juce::Justification::centred);
            label->setFont(juce::FontOptions(13.0f, juce::Font::bold));
            label->setEditable(!isLocked);
            label->setColour(juce::Label::backgroundColourId, juce::Colour(0xfff8fafc));
            label->setColour(juce::Label::outlineColourId, juce::Colour(0xffe2e8f0));
            label->setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
            label->setColour(juce::Label::textWhenEditingColourId, juce::Colour(0xff0f172a));

            label->onEditorShow = [this, beat] { focusBeat(beat); };
            label->onEditorHide = [this] {
                isExpanded = false;
                resized();
                repaint();
            };
            label->onTextChange = [this, beat, labelPtr = label.get()] {
                auto text = labelPtr->getText().trim();
                currentMeasure.ensureBeatChords(beatsPerMeasure);

                if (text.isEmpty()) {
                    currentMeasure.beatChords[static_cast<size_t>(beat)] = Chord();
                    labelPtr->setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
                    if (onBeatChordValidated) onBeatChordValidated(index, beat, Chord());
                    return;
                }

                int durationBeats = 1;
                juce::String chordText = text;
                const int slashIndex = text.indexOfChar('/');
                if (slashIndex >= 0) {
                    chordText = text.substring(0, slashIndex).trim();
                    auto durationText = text.substring(slashIndex + 1).trim();
                    durationBeats = durationText.getIntValue();
                }

                if (chordText.isEmpty()) {
                    labelPtr->setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
                    return;
                }

                auto parsed = Chord::fromString(chordText);
                const int remainingBeats = beatsPerMeasure - beat;
                if (parsed.isEmpty() || durationBeats < 1 || durationBeats > beatsPerMeasure) {
                    labelPtr->setColour(juce::Label::textColourId, juce::Colour(0xffef4444));
                    return;
                }

                durationBeats = std::min(durationBeats, remainingBeats);
                for (int targetBeat = beat; targetBeat < beat + durationBeats; ++targetBeat) {
                    currentMeasure.beatChords[static_cast<size_t>(targetBeat)] = parsed;
                    if (onBeatChordValidated) onBeatChordValidated(index, targetBeat, parsed);
                }

                syncLabelsFromMeasure();
                labelPtr->setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));
            };

            addAndMakeVisible(*label);
            beatLabels.push_back(std::move(label));
        }
        resized();
    }

    void syncLabelsFromMeasure() {
        for (int beat = 0; beat < beatsPerMeasure; ++beat) {
            const auto& chord = currentMeasure.beatChords[static_cast<size_t>(beat)];
            beatLabels[static_cast<size_t>(beat)]->setText(chord.name, juce::dontSendNotification);
            beatLabels[static_cast<size_t>(beat)]->setColour(juce::Label::textColourId,
                                                            chord.isEmpty() ? juce::Colour(0xff94a3b8) : juce::Colour(0xff0f172a));
        }
    }

    bool isValidBeat(int beat) const { return beat >= 0 && beat < beatsPerMeasure; }

    int beatIndexFromPosition(float x) const {
        for (int beat = 0; beat < static_cast<int>(beatLabels.size()); ++beat) {
            if (beatLabels[static_cast<size_t>(beat)]->getBounds().contains(juce::Point<int>(static_cast<int>(x), getHeight() / 2))) {
                return beat;
            }
        }
        return std::clamp(static_cast<int>((x / std::max(1, getWidth())) * beatsPerMeasure), 0, beatsPerMeasure - 1);
    }

    void focusBeat(int beat) {
        if (!isValidBeat(beat)) return;
        activeBeatIndex = beat;
        if (onFocusGained) onFocusGained(index, beat);
        repaint();
    }

    int index;
    int beatsPerMeasure;
    int activeBeatIndex = -1;
    Measure currentMeasure;
    std::vector<std::unique_ptr<juce::Label>> beatLabels;
    bool isPlaying = false;
    bool isLocked = false;
    bool isExpanded = false;
};

class GridCanvas : public juce::Component {
public:
    std::function<void(const Progression&)> onProgressionChanged;

    GridCanvas() {}

    void loadProgression(const Progression& prog) {
        currentProg = prog;
        beatsPerMeasure = calculateChordSlots(currentProg);
        boxes.clear();

        for (auto& measure : currentProg.measures) {
            measure.ensureBeatChords(beatsPerMeasure);
        }

        for (size_t i = 0; i < currentProg.measures.size(); ++i) {
            auto box = std::make_unique<MeasureBox>(static_cast<int>(i), beatsPerMeasure);
            box->setMeasure(currentProg.measures[i], beatsPerMeasure);
            box->setLocked(isLocked);

            box->onFocusGained = [this](int measureIdx, int beatIdx) {
                cursorMeasureIndex = measureIdx;
                cursorBeatIndex = beatIdx;
                updateActiveBox();
            };

            box->onBeatChordValidated = [this](int measureIdx, int beatIdx, const Chord& chord) {
                if (!isValidCursor(measureIdx, beatIdx)) return;
                currentProg.measures[static_cast<size_t>(measureIdx)].ensureBeatChords(beatsPerMeasure);
                currentProg.measures[static_cast<size_t>(measureIdx)].beatChords[static_cast<size_t>(beatIdx)] = chord;
                currentProg.measures[static_cast<size_t>(measureIdx)].chord = firstNonEmptyChord(currentProg.measures[static_cast<size_t>(measureIdx)]);
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
            cursorMeasureIndex = -1;
            cursorBeatIndex = -1;
            updateActiveBox();
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
    static int calculateChordSlots(const Progression& prog) {
        return std::max(1, static_cast<int>(std::round(prog.getBeatsPerMeasure())));
    }

    bool isValidCursor(int measureIdx, int beatIdx) const {
        return measureIdx >= 0 && beatIdx >= 0
            && measureIdx < static_cast<int>(currentProg.measures.size())
            && beatIdx < beatsPerMeasure;
    }

    Chord firstNonEmptyChord(const Measure& measure) const {
        for (const auto& chord : measure.beatChords) {
            if (!chord.isEmpty()) return chord;
        }
        return Chord();
    }

    void updateActiveBox() {
        for (size_t i = 0; i < boxes.size(); ++i) {
            if (static_cast<int>(i) == cursorMeasureIndex) boxes[i]->setActiveBeat(cursorBeatIndex);
            else boxes[i]->clearActiveBeat();
        }
    }

    std::vector<std::unique_ptr<MeasureBox>> boxes;
    Progression currentProg;
    int beatsPerMeasure = 4;
    int cursorMeasureIndex = -1;
    int cursorBeatIndex = -1;
    bool isLocked = false;
};
