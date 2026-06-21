#pragma once
#include "IInferenceEngine.h"
#include "Markov/MarkovManager.h"
#include <string>

class MarkovEngine : public IInferenceEngine {
public:
    MarkovEngine();
    ~MarkovEngine() override = default;

    void processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                       std::vector<MidiEventLite>& outputPhrase,
                       const Chord& currentChord,
                       juce::int64 responseStartSample,
                       juce::int64 responseLengthSamples,
                       double sampleRate,
                       double bpm) override;

    // 新增缺失的接口声明
    bool loadModel(const juce::File& modelFile) override;
    bool isModelReady() const override;
    void resetModelMemory() override;

private:
    MarkovManager markovManager;
    bool modelReady = false;
};