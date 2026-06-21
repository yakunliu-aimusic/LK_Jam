#pragma once
#include "IInferenceEngine.h"
#include <vector>
#include <atomic>

class RTNeuralEngine : public IInferenceEngine {
public:
    RTNeuralEngine();
    ~RTNeuralEngine() override = default;

    void processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                       std::vector<MidiEventLite>& outputPhrase,
                       const Chord& currentChord,
                       juce::int64 responseStartSample,
                       juce::int64 responseLengthSamples,
                       double sampleRate,
                       double bpm) override;

    // 🌟 核心修复点 2：实现父类的纯虚函数，使其可以被实例化
    bool loadModel(const juce::File& modelFile) override;
    bool isModelReady() const override { return modelReady.load(); }

private:
    std::vector<float> preprocessData(const std::vector<MidiEventLite>& input, const Chord& chord);
    std::vector<float> runModelInference(const std::vector<float>& inputFeatures);
    std::vector<MidiEventLite> postprocessData(const std::vector<float>& modelOutput, juce::int64 startSample);

    std::atomic<bool> modelReady{false};
    juce::String currentModelName{"None"};
};