#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Data/HarmonyData.h"
#include <functional>

class ChordDeck : public juce::Component {
public:
    // 回调函数：当用户组合好一个和弦并确认输入时触发
    std::function<void(int rootMidi, ChordQuality quality, juce::String name)> onChordSelected;
    // 回调函数：当用户点击删除时触发
    std::function<void()> onDeleteClicked;

    ChordDeck() {
        // 1. 根音按钮初始化
        const juce::StringArray rootNames = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
        for (int i = 0; i < 12; ++i) {
            auto btn = std::make_unique<juce::TextButton>(rootNames[i]);
            btn->setClickingTogglesState(true);
            btn->setRadioGroupId(1); // 确保同一时间只能选中一个根音
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff1f5f9));
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3b82f6)); // 选中变蓝
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btn->onClick = [this, i] { handleRootSelection(i); };
            addAndMakeVisible(*btn);
            rootButtons.push_back(std::move(btn));
        }

        // 2. 属性按钮初始化
        const std::vector<std::pair<juce::String, ChordQuality>> qualities = {
            {"Major", ChordQuality::Major}, {"m", ChordQuality::Minor},
            {"maj7", ChordQuality::Maj7},   {"m7", ChordQuality::Min7},
            {"7", ChordQuality::Dom7},      {"m7b5", ChordQuality::Min7b5},
            {"dim7", ChordQuality::Dim7},   {"aug", ChordQuality::Aug},
            {"7#9", ChordQuality::Dom7Sharp9}, {"7b9", ChordQuality::Dom7Flat9}
        };

        for (const auto& q : qualities) {
            auto btn = std::make_unique<juce::TextButton>(q.first);
            btn->setClickingTogglesState(true);
            btn->setRadioGroupId(2); // 属性按钮专属 Group
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff1f5f9));
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff59e0b)); // 选中变黄
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff64748b));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btn->onClick = [this, q] { handleQualitySelection(q.second); };
            addAndMakeVisible(*btn);
            qualityButtons.push_back(std::move(btn));
        }

        // 3. 删除/重置按钮
        deleteBtn.setButtonText("DEL");
        deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfffee2e2));
        deleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffef4444));
        deleteBtn.onClick = [this] { if (onDeleteClicked) onDeleteClicked(); };
        addAndMakeVisible(deleteBtn);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(8);

        // 分为上下两行
        auto row1 = bounds.removeFromTop(bounds.getHeight() / 2).reduced(0, 4);
        auto row2 = bounds.reduced(0, 4);

        // 排布根音按钮 (第一行)
        int rootWidth = row1.getWidth() / 12;
        for (auto& btn : rootButtons) {
            btn->setBounds(row1.removeFromLeft(rootWidth).reduced(2, 0));
        }

        // 排布属性按钮和删除按钮 (第二行)
        int qualWidth = row2.getWidth() / (qualityButtons.size() + 1);
        for (auto& btn : qualityButtons) {
            btn->setBounds(row2.removeFromLeft(qualWidth).reduced(2, 0));
        }
        deleteBtn.setBounds(row2.reduced(2, 0));
    }

    // 重置按钮的选中状态（通常在光标移动到新小节时调用）
    void resetSelection() {
        for (auto& btn : rootButtons) btn->setToggleState(false, juce::dontSendNotification);
        for (auto& btn : qualityButtons) btn->setToggleState(false, juce::dontSendNotification);
        currentRoot = -1;
        currentQuality = ChordQuality::None;
    }

private:
    void handleRootSelection(int rootMidi) {
        currentRoot = rootMidi;
        checkAndDispatch();
    }

    void handleQualitySelection(ChordQuality quality) {
        currentQuality = quality;
        checkAndDispatch();
    }

    void checkAndDispatch() {
        // 只有当根音和属性都被选中时，才组合成和弦并发送给网格
        if (currentRoot != -1 && currentQuality != ChordQuality::None) {
            juce::String rootName = getRootName(currentRoot);
            juce::String qualName = getQualityName(currentQuality);
            juce::String fullChordName = rootName + qualName;

            if (onChordSelected) {
                onChordSelected(currentRoot, currentQuality, fullChordName);
            }

            // 发送完毕后，等待主网格更新光标，这里不做重置，由主网格控制
        }
    }

    juce::String getRootName(int midi) {
        const juce::StringArray names = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
        if (midi >= 0 && midi < 12) return names[midi];
        return "";
    }

    juce::String getQualityName(ChordQuality q) {
        switch (q) {
            case ChordQuality::Major: return ""; // 大三和弦通常省略后缀，如 "C"
            case ChordQuality::Minor: return "m";
            case ChordQuality::Maj7:  return "maj7";
            case ChordQuality::Min7:  return "m7";
            case ChordQuality::Dom7:  return "7";
            case ChordQuality::Min7b5:return "m7b5";
            case ChordQuality::Dim7:  return "dim7";
            case ChordQuality::Aug:   return "aug";
            case ChordQuality::Aug7:  return "aug7";
            case ChordQuality::Dom7Sharp9: return "7#9";
            case ChordQuality::Dom7Flat9:  return "7b9";
            default: return "";
        }
    }

    std::vector<std::unique_ptr<juce::TextButton>> rootButtons;
    std::vector<std::unique_ptr<juce::TextButton>> qualityButtons;
    juce::TextButton deleteBtn;

    int currentRoot = -1;
    ChordQuality currentQuality = ChordQuality::None;
};