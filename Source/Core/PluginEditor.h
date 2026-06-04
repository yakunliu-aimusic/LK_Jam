#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// 引入你的 UI 面板组件
#include "../UI/GlobalHostPanel.h"
#include "../UI/InteractiveDisplay.h"
#include "../UI/AIControlsPanel.h"
#include "../UI/SystemStatusPanel.h"

class LK_Jam_POCEditor : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    LK_Jam_POCEditor(LK_Jam_POCProcessor&);
    ~LK_Jam_POCEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LK_Jam_POCProcessor& audioProcessor;

    // 四大核心面板
    GlobalHostPanel globalPanel;
    InteractiveDisplay interactivePanel;
    AIControlsPanel aiControlsPanel;
    SystemStatusPanel systemPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LK_Jam_POCEditor)
};