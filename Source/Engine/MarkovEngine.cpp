#include "MarkovEngine.h"
#include <algorithm>

MarkovEngine::MarkovEngine() : markovManager(4, 20) {
}

bool MarkovEngine::loadModel(const juce::File& modelFile) {
    juce::ignoreUnused(modelFile);
    modelReady = true;
    return true;
}

bool MarkovEngine::isModelReady() const {
    return modelReady;
}

void MarkovEngine::processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                                 std::vector<MidiEventLite>& outputPhrase,
                                 const Chord& currentChord,
                                 juce::int64 responseStartSample) {
    juce::ignoreUnused(currentChord);

    for (const auto& ev : inputPhrase) {
        if (ev.isNoteOn) {
            markovManager.putEvent(std::to_string(ev.pitch));
        }
    }

    juce::int64 currentSample = responseStartSample;
    int noteDurationSamples = 44100 / 4;
    int numNotesToGenerate = 8;

    for (int i = 0; i < numNotesToGenerate; ++i) {
        std::string nextPitchStr = markovManager.getEvent(true, false);

        if (nextPitchStr != "0" && !nextPitchStr.empty()) {
            try {
                int pitch = std::stoi(nextPitchStr);
                pitch = std::clamp(pitch, 0, 127);

                outputPhrase.push_back({(int)currentSample, pitch, 80, true});
                outputPhrase.push_back({(int)(currentSample + noteDurationSamples), pitch, 0, false});

                currentSample += noteDurationSamples;
            } catch (...) {
                continue;
            }
        }
    }
}