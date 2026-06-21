#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/PluginProcessor.h"
#include "BinaryData.h"  // <--- 新增这一行！
#include <atomic>

class GlobalHostPanel : public juce::Component {
public:
    bool isAlive() const { return bIsAlive.load(); }

    explicit GlobalHostPanel(LK_Jam_POCProcessor& p) : processor(p) {
        bIsAlive.store(true);

        // --- 模块 2：时钟同步 ---
        clockSourceTitle.setText("CLOCK SOURCE", juce::dontSendNotification);
        clockSourceTitle.setFont(juce::FontOptions(14.0f, juce::Font::plain));
        clockSourceTitle.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        clockSourceTitle.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(clockSourceTitle);

        clockSourceCombo.addItem("Host Sync", 1);
        clockSourceCombo.addItem("Internal Clock", 2);
        clockSourceCombo.addItem("MIDI Clock In", 3);
        clockSourceCombo.setSelectedId(1, juce::dontSendNotification);
        clockSourceCombo.setJustificationType(juce::Justification::centred);
        clockSourceCombo.onChange = [this]() {
            bool isInternal = (clockSourceCombo.getSelectedId() == 2);
            bpmSlider.setVisible(isInternal);
            bpmLabel.setColour(juce::Label::textColourId, isInternal ? juce::Colour(0xff0f172a) : juce::Colour(0xff94a3b8));
            processor.getSyncEngine().setSyncMode(clockSourceCombo.getSelectedId());
        };
        addAndMakeVisible(clockSourceCombo);

        // --- 模块 3：BPM 控制 ---
        bpmLabel.setText("120.0", juce::dontSendNotification);
        bpmLabel.setJustificationType(juce::Justification::centred);
        bpmLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
        bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
        addAndMakeVisible(bpmLabel);

        bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        bpmSlider.setRange(60.0, 240.0, 0.5);
        bpmSlider.setValue(120.0);
        bpmSlider.setVisible(false);
        bpmSlider.onValueChange = [this]() {
            bpmLabel.setText(juce::String::formatted("%.1f", bpmSlider.getValue()), juce::dontSendNotification);
            processor.currentBpm.store(bpmSlider.getValue());
            if (clockSourceCombo.getSelectedId() == 2) {
                processor.getSyncEngine().setManualBpm(bpmSlider.getValue());
            }
        };
        addAndMakeVisible(bpmSlider);

        // --- 模块 4：全局传输控制 ---
        playStopBtn.setButtonText("PLAY");
        playStopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b82f6));
        playStopBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        playStopBtn.onClick = [this]() {
            const bool newState = !processor.isTransportRunning.load();
            processor.isTransportRunning.store(newState);
            if (newState) {
                processor.resetLoopState();
            } else {
                processor.panicTriggered.store(true);
            }
            playStopBtn.setButtonText(newState ? "STOP" : "PLAY");
            playStopBtn.setColour(juce::TextButton::buttonColourId, newState ? juce::Colour(0xffef4444) : juce::Colour(0xff3b82f6));
        };
        addAndMakeVisible(playStopBtn);

        learningBtn.setButtonText("LEARNING");
        learningBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff64748b));
        learningBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        learningBtn.onClick = [this]() {
            const bool newState = !processor.isLearning.load();
            processor.isLearning.store(newState);
            learningBtn.setColour(juce::TextButton::buttonColourId, newState ? juce::Colour(0xff10b981) : juce::Colour(0xff64748b));
        };
        addAndMakeVisible(learningBtn);

        panicBtn.setButtonText("ALL NOTES OFF");
        panicBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
        panicBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));
        panicBtn.onClick = [this]() { processor.panicTriggered.store(true); };
        addAndMakeVisible(panicBtn);

        // --- 模块 5：AI 状态区 ---
        aiCoreLabel.setText("AI CORE: RUNNING", juce::dontSendNotification);
        aiCoreLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        aiCoreLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        addAndMakeVisible(aiCoreLabel);

        phaseLabel.setText("PHASE: Listening", juce::dontSendNotification);
        phaseLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        phaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(phaseLabel);

        modelLabel.setText("MODEL: Jazz (128k)", juce::dontSendNotification);
        modelLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        modelLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(modelLabel);

        latencyLabel.setText("LATENCY: 0.1ms", juce::dontSendNotification);
        latencyLabel.setFont(juce::FontOptions(13.0f, juce::Font::plain));
        latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff64748b));
        addAndMakeVisible(latencyLabel);
    }

    ~GlobalHostPanel() override { bIsAlive.store(false); }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xffffffff));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(0xffcbd5e1));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        auto headerBounds = bounds.removeFromTop(70);

        // --- 1. 设置字体并获取文字的真实宽度 ---
        juce::Font titleFont(juce::FontOptions(36.0f, juce::Font::bold));
        g.setFont(titleFont);
        juce::String titleText = "LK Jam";
        juce::GlyphArrangement titleGlyphs;
        titleGlyphs.addLineOfText(titleFont, titleText, 0.0f, 0.0f);
        float textWidth = titleGlyphs.getBoundingBox(0, -1, true).getWidth();

        // --- 2. 设定 Logo 大小和极近的间距 ---
        float logoSize = 40.0f;
        float spacing = 8.0f; // 间距改小到 8 个像素，让它俩贴得更近

        // --- 3. 计算“文字+间距+Logo”的总宽度，求出居中起点 X ---
        float totalWidth = textWidth + spacing + logoSize;
        float startX = headerBounds.getX() + (headerBounds.getWidth() - totalWidth) / 2.0f;
        float centerY = headerBounds.getCentreY();

        // --- 4. 绘制文字 ---
        juce::Rectangle<float> textBounds(startX, headerBounds.getY(), textWidth, headerBounds.getHeight());
        g.setColour(juce::Colour(0xff0f172a));
        g.drawText(titleText, textBounds, juce::Justification::centredLeft);

        // --- 5. 绘制 Logo ---
        juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_jpg, BinaryData::logo_jpgSize);
        if (logo.isValid())
        {
            // Logo 的 X 坐标紧跟在文字和间距之后
            float logoX = startX + textWidth + spacing;
            float logoY = centerY - (logoSize / 2.0f); // 垂直居中
            juce::Rectangle<float> imageBounds(logoX, logoY, logoSize, logoSize);

            g.drawImage (logo,
                         imageBounds,
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }

        // --- 绘制底部分隔线 ---
        g.setColour(juce::Colour(0xfff1f5f9));
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getY(), 2.0f);
    }
    void resized() override {
        auto bounds = getLocalBounds().reduced(20).withTrimmedTop(82);

        clockSourceTitle.setBounds(bounds.removeFromTop(20));
        clockSourceCombo.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(16);

        bpmLabel.setBounds(bounds.removeFromTop(40));
        bpmSlider.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(20);

        playStopBtn.setBounds(bounds.removeFromTop(48));
        bounds.removeFromTop(8);
        learningBtn.setBounds(bounds.removeFromTop(48));
        bounds.removeFromTop(8);
        panicBtn.setBounds(bounds.removeFromTop(48));

        bounds.removeFromTop(20);

        aiCoreLabel.setBounds(bounds.removeFromTop(20));
        phaseLabel.setBounds(bounds.removeFromTop(20));
        modelLabel.setBounds(bounds.removeFromTop(20));
        latencyLabel.setBounds(bounds.removeFromTop(20));
    }

    void updateStateSafe() { if (isAlive()) updateState(); }

private:
    void updateState() {
        const double bpm = processor.currentBpm.load();
        const double latency = processor.currentLatencyMs.load();
        const int stateInt = processor.getStateMachine().getStateAsInt();
        const bool transportRunning = processor.isTransportRunning.load();
        const bool learning = processor.isLearning.load();

        juce::MessageManager::callAsync([=]() {
            if (!isAlive()) return;
            if (clockSourceCombo.getSelectedId() != 2) bpmLabel.setText(juce::String::formatted("%.1f", bpm), juce::dontSendNotification);
            playStopBtn.setButtonText(transportRunning ? "STOP" : "PLAY");
            playStopBtn.setColour(juce::TextButton::buttonColourId, transportRunning ? juce::Colour(0xffef4444) : juce::Colour(0xff3b82f6));
            learningBtn.setColour(juce::TextButton::buttonColourId, learning ? juce::Colour(0xff10b981) : juce::Colour(0xff64748b));
            latencyLabel.setText(juce::String::formatted("LATENCY: %.1fms", latency), juce::dontSendNotification);
            if (stateInt == 1) phaseLabel.setText("PHASE: Listening", juce::dontSendNotification);
            else if (stateInt == 2) phaseLabel.setText("PHASE: Responding", juce::dontSendNotification);
            else if (stateInt == 3) phaseLabel.setText("PHASE: Pre-Roll", juce::dontSendNotification);
            else phaseLabel.setText("PHASE: Idle", juce::dontSendNotification);
        });
    }

    std::atomic<bool> bIsAlive {false};
    LK_Jam_POCProcessor& processor;

    juce::Label clockSourceTitle, bpmLabel, aiCoreLabel, phaseLabel, modelLabel, latencyLabel;
    juce::ComboBox clockSourceCombo;
    juce::Slider bpmSlider;
    juce::TextButton playStopBtn, learningBtn, panicBtn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalHostPanel)
};