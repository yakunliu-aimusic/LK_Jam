#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <juce_core/juce_core.h>

enum class ChordQuality {
    None, Major, Minor, Maj7, Min7, Dom7, Min7b5, Dim7, Aug, Aug7,
    Dom7Sharp9, Dom7Flat9, Maj9, Min9, Dom9
};

struct Chord {
    int rootMidi = -1;
    ChordQuality quality = ChordQuality::None;
    juce::String name = "";

    std::vector<int> getIntervals() const {
        switch (quality) {
            case ChordQuality::Major:      return {0, 4, 7};
            case ChordQuality::Minor:      return {0, 3, 7};
            case ChordQuality::Maj7:       return {0, 4, 7, 11};
            case ChordQuality::Min7:       return {0, 3, 7, 10};
            case ChordQuality::Dom7:       return {0, 4, 7, 10};
            case ChordQuality::Min7b5:     return {0, 3, 6, 10};
            case ChordQuality::Dim7:       return {0, 3, 6, 9};
            case ChordQuality::Aug:        return {0, 4, 8};
            case ChordQuality::Aug7:       return {0, 4, 8, 10};
            case ChordQuality::Dom7Sharp9: return {0, 4, 7, 10, 15};
            case ChordQuality::Dom7Flat9:  return {0, 4, 7, 10, 13};
            case ChordQuality::Maj9:       return {0, 4, 7, 11, 14};
            case ChordQuality::Min9:       return {0, 3, 7, 10, 14};
            case ChordQuality::Dom9:       return {0, 4, 7, 10, 14};
            case ChordQuality::None:       return {};
            default: return {};
        }
    }

    bool isEmpty() const { return rootMidi == -1 || quality == ChordQuality::None; }

    static Chord fromString(const juce::String& input) {
        Chord c;
        juce::String str = input.trim();
        if (str.isEmpty()) return c;

        juce::String rootStr = str.substring(0, 1).toUpperCase();
        int offset = 1;
        if (str.length() > 1) {
            const juce::String accidental = str.substring(1, 2);
            if (accidental == "#" || accidental == "b") {
                rootStr += accidental.toLowerCase();
                offset = 2;
            }
        }

        if (rootStr == "C") c.rootMidi = 0;
        else if (rootStr == "C#" || rootStr == "Db") c.rootMidi = 1;
        else if (rootStr == "D") c.rootMidi = 2;
        else if (rootStr == "D#" || rootStr == "Eb") c.rootMidi = 3;
        else if (rootStr == "E") c.rootMidi = 4;
        else if (rootStr == "F") c.rootMidi = 5;
        else if (rootStr == "F#" || rootStr == "Gb") c.rootMidi = 6;
        else if (rootStr == "G") c.rootMidi = 7;
        else if (rootStr == "G#" || rootStr == "Ab") c.rootMidi = 8;
        else if (rootStr == "A") c.rootMidi = 9;
        else if (rootStr == "A#" || rootStr == "Bb") c.rootMidi = 10;
        else if (rootStr == "B") c.rootMidi = 11;
        else return c;

        // 🌟 核心修复：提取原始后缀保留大小写，同时保留一个全小写副本用于通用匹配
        juce::String rawQualStr = str.substring(offset).trim();
        juce::String lowerQualStr = rawQualStr.toLowerCase();

        c.quality = ChordQuality::Major;
        juce::String displayQual = "";

        if (rawQualStr == "M7" || lowerQualStr == "maj7" || lowerQualStr == "maj") {
            c.quality = ChordQuality::Maj7; displayQual = "maj7";
        }
        else if (rawQualStr == "m" || lowerQualStr == "min") {
            c.quality = ChordQuality::Minor; displayQual = "m";
        }
        else if (rawQualStr == "m7" || lowerQualStr == "min7") {
            c.quality = ChordQuality::Min7; displayQual = "m7";
        }
        else if (lowerQualStr == "7" || lowerQualStr == "dom7") {
            c.quality = ChordQuality::Dom7; displayQual = "7";
        }
        else if (lowerQualStr == "m7b5") {
            c.quality = ChordQuality::Min7b5; displayQual = "m7b5";
        }
        else if (lowerQualStr == "dim" || lowerQualStr == "dim7") {
            c.quality = ChordQuality::Dim7; displayQual = "dim7";
        }
        else if (lowerQualStr == "aug") {
            c.quality = ChordQuality::Aug; displayQual = "aug";
        }
        else if (lowerQualStr == "aug7") {
            c.quality = ChordQuality::Aug7; displayQual = "aug7";
        }
        else if (lowerQualStr == "7#9") {
            c.quality = ChordQuality::Dom7Sharp9; displayQual = "7#9";
        }
        else if (lowerQualStr == "7b9") {
            c.quality = ChordQuality::Dom7Flat9; displayQual = "7b9";
        }
        else if (rawQualStr.isNotEmpty()) {
            return Chord(); // 后缀不为空但未能匹配任何合法和弦，返回非法状态
        }

        c.name = rootStr + displayQual;
        return c;
    }
};

struct Measure {
    Chord chord;
    std::vector<Chord> beatChords;

    void ensureBeatChords(int beatsPerMeasure) {
        const int safeBeats = std::max(1, beatsPerMeasure);
        if (beatChords.size() != static_cast<size_t>(safeBeats)) {
            beatChords.resize(static_cast<size_t>(safeBeats));
        }
    }

    Chord getChordForBeat(int beatIndex, int beatsPerMeasure) const {
        const int safeBeats = std::max(1, beatsPerMeasure);
        const int safeBeat = std::clamp(beatIndex, 0, safeBeats - 1);
        if (beatChords.size() == static_cast<size_t>(safeBeats) && !beatChords[static_cast<size_t>(safeBeat)].isEmpty()) {
            return beatChords[static_cast<size_t>(safeBeat)];
        }
        return chord;
    }
};

struct Progression {
    juce::String name;
    std::vector<Measure> measures;

    int timeSigNum = 4;
    int timeSigDen = 4;
    int totalLoops = 0;

    Progression(juce::String n = "New Song", int numMeasures = 16) : name(n) {
        measures.resize(static_cast<size_t>(numMeasures));
    }

    double getBeatsPerMeasure() const {
        return static_cast<double>(timeSigNum) * (4.0 / static_cast<double>(timeSigDen));
    }

    double getTotalBeats() const {
        return measures.size() * getBeatsPerMeasure();
    }
};

class HarmonyLibrary {
public:
    static std::vector<Progression> getPresets() {
        std::vector<Progression> presets;
        presets.push_back(Progression("Blank (16 Bars)", 16));

        Progression blues("Jazz Blues (Bb)", 12);
        blues.measures[0].chord = Chord::fromString("Bb7");
        blues.measures[1].chord = Chord::fromString("Eb7");
        blues.measures[2].chord = Chord::fromString("Bb7");
        blues.measures[3].chord = Chord::fromString("Bb7");
        blues.measures[4].chord = Chord::fromString("Eb7");
        blues.measures[5].chord = Chord::fromString("Edim7");
        blues.measures[6].chord = Chord::fromString("Bb7");
        blues.measures[7].chord = Chord::fromString("G7");
        blues.measures[8].chord = Chord::fromString("Cm7");
        blues.measures[9].chord = Chord::fromString("F7");
        blues.measures[10].chord = Chord::fromString("Bb7");
        blues.measures[11].chord = Chord::fromString("F7");
        presets.push_back(blues);

        return presets;
    }
};