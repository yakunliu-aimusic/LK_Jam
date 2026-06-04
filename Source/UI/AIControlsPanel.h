#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/PluginProcessor.h"

// --- 1. 全新的高级现代旋钮样式 (带外环轨道和内部指示点) ---
class ModernKnobLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& /*slider*/) override {

        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // 1. 绘制外部进度轨道
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xffe2e8f0));
        g.strokePath(trackArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. 绘制当前值进度条
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xff6366f1));
        g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. 绘制旋钮主体
        auto innerRadius = radius - 8.0f;
        g.setColour(juce::Colour(0xffffffff));
        g.fillEllipse(centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawEllipse(centreX - innerRadius, centreY - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.5f);

        // 4. 绘制指示圆点
        auto dotRadius = 3.5f;
        auto dotDistance = innerRadius - 7.0f;
        auto dotX = centreX + dotDistance * std::cos(angle - juce::MathConstants<float>::halfPi);
        auto dotY = centreY + dotDistance * std::sin(angle - juce::MathConstants<float>::halfPi);

        g.setColour(juce::Colour(0xff0f172a));
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
    }
};

// --- 2. 主面板类 ---
class AIControlsPanel : public juce::Component {
public:
    explicit AIControlsPanel(LK_Jam_POCProcessor& p) : processor(p) {

        // --- Load AI Model 按钮 ---
        loadModelBtn.setButtonText("Load AI Model...");
        loadModelBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
        loadModelBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));
        loadModelBtn.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffcbd5e1));
        loadModelBtn.onClick = [this]() {
            fileChooser = std::make_unique<juce::FileChooser>(
                "Select AI Model", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.json;*.bin"
            );
            auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser) {
                juce::File result = chooser.getResult();
                if (result.exists()) {
                    processor.loadCustomModel(result);
                }
            });
        };
        addAndMakeVisible(loadModelBtn);

        // --- Model 选择 --- ✅ 已修改
        modelBox.setJustificationType(juce::Justification::centred);
        modelBox.addItem("None (Fallback Only)", 1);
        modelBox.addItem("Markov Chain Engine", 2);
        modelBox.addItem("GRU Neural Net (WIP)", 3);
        modelBox.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(modelBox);

        // --- Style 选择 ---
        styleBox.setJustificationType(juce::Justification::centred);
        styleBox.addItem("Jazz", 1);
        styleBox.addItem("Pop", 2);
        styleBox.addItem("Experimental", 3);
        styleBox.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(styleBox);

        // --- Fallback 按钮 ---
        fallbackToggle.setClickingTogglesState(true);
        fallbackToggle.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff1f5f9));
        fallbackToggle.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
        fallbackToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff10b981));
        fallbackToggle.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        fallbackToggle.onClick = [this]() {
            fallbackToggle.setButtonText(fallbackToggle.getToggleState() ? "Fallback Mode: ON" : "Fallback Mode: OFF");
        };
        addAndMakeVisible(fallbackToggle);

        // --- Temp 旋钮 ---
        tempKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        tempKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        tempKnob.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff0f172a));
        tempKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        tempKnob.setLookAndFeel(&modernKnob);
        addAndMakeVisible(tempKnob);

        tempLabel.setText("Temp", juce::dontSendNotification);
        tempLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        tempLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        tempLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(tempLabel);

        // --- Top-P 旋钮 ---
        topPKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        topPKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        topPKnob.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff0f172a));
        topPKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        topPKnob.setLookAndFeel(&modernKnob);
        addAndMakeVisible(topPKnob);

        topPLabel.setText("Top-P", juce::dontSendNotification);
        topPLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        topPLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        topPLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(topPLabel);

        // --- APVTS 绑定 ---
        modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "modelChoice", modelBox);
        styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "styleChoice", styleBox);
        tempAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "temperature", tempKnob);
        topPAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "topP", topPKnob);
        fallbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "fallbackMode", fallbackToggle);

        fallbackToggle.setButtonText(fallbackToggle.getToggleState() ? "Fallback Mode: ON" : "Fallback Mode: OFF");
    }

    ~AIControlsPanel() override {
        tempKnob.setLookAndFeel(nullptr);
        topPKnob.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        g.setColour(juce::Colour(0xff64748b));
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));

        auto titleArea = bounds.removeFromTop(60).withTrimmedTop(20);
        g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
        g.drawText("AI  MODEL", titleArea, juce::Justification::centredTop);
        g.drawText("PARAMETERS", titleArea, juce::Justification::centredBottom);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(24).withTrimmedTop(80);

        loadModelBtn.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(28);

        modelBox.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(28);

        styleBox.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(28);

        fallbackToggle.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(32);

        auto knobRow = bounds.removeFromTop(130);
        auto leftKnobArea = knobRow.removeFromLeft(knobRow.getWidth() / 2);

        tempLabel.setBounds(leftKnobArea.removeFromTop(22));
        tempKnob.setBounds(leftKnobArea.reduced(6));

        topPLabel.setBounds(knobRow.removeFromTop(22));
        topPKnob.setBounds(knobRow.reduced(6));
    }

private:
    LK_Jam_POCProcessor& processor;
    ModernKnobLookAndFeel modernKnob;

    juce::TextButton loadModelBtn;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::ComboBox modelBox;
    juce::ComboBox styleBox;
    juce::TextButton fallbackToggle;
    juce::Slider tempKnob, topPKnob;
    juce::Label tempLabel, topPLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> topPAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fallbackAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIControlsPanel)
};