#include "PluginEditor.h"

LK_Jam_POCEditor::LK_Jam_POCEditor(LK_Jam_POCProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      globalPanel(p),
      interactivePanel(p),
      aiControlsPanel(p),  // <--- This is the crucial fix! It must be just 'p'
      systemPanel(p)
{
    setSize(1000, 600);

    // ==========================================
    // 🎨 全局【极简扁平 & 淡黄点缀】统一主题
    // ==========================================
    auto& lf = getLookAndFeel();

    // 按钮：纯白背景 + 深灰文字
    lf.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffffff));
    lf.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff0f172a));

    // 下拉框 ComboBox：纯白 + 细边框 + 深灰文字
    lf.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xffffffff));
    lf.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffcbd5e1));
    lf.setColour(juce::ComboBox::textColourId, juce::Colour(0xff0f172a));
    lf.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff64748b));

    // 旋钮 Slider：统一淡黄高亮 + 深灰旋钮头
    lf.setColour(juce::Slider::thumbColourId, juce::Colour(0xff0f172a));
    lf.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xfffcd34d)); // 淡奶油黄
    lf.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffe2e8f0));

    // 所有文字标签统一深灰
    lf.setColour(juce::Label::textColourId, juce::Colour(0xff0f172a));

    // 添加面板
    addAndMakeVisible(globalPanel);
    addAndMakeVisible(interactivePanel);
    addAndMakeVisible(aiControlsPanel);
    addAndMakeVisible(systemPanel);

    startTimerHz(30);
}

LK_Jam_POCEditor::~LK_Jam_POCEditor() {
    stopTimer();
}

void LK_Jam_POCEditor::paint(juce::Graphics& g) {
    // 主背景：干净冷调浅灰蓝
    g.fillAll(juce::Colour(0xfff1f5f9));
}

void LK_Jam_POCEditor::resized() {
    auto bounds = getLocalBounds().reduced(16);

    systemPanel.setBounds(bounds.removeFromBottom(48));
    bounds.removeFromBottom(16);

    globalPanel.setBounds(bounds.removeFromLeft(224));
    bounds.removeFromLeft(16);

    aiControlsPanel.setBounds(bounds.removeFromRight(256));
    bounds.removeFromRight(16);

    interactivePanel.setBounds(bounds);
}

void LK_Jam_POCEditor::timerCallback() {
    if (globalPanel.isAlive()) globalPanel.updateStateSafe();
    if (interactivePanel.isAlive()) interactivePanel.updateStateSafe();
    if (systemPanel.isAlive()) systemPanel.updateStateSafe();
}