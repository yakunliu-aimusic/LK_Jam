#include "RTNeuralEngine.h"

RTNeuralEngine::RTNeuralEngine() {}

// 🌟 核心修复点：补充了 loadModel 的具体实现
bool RTNeuralEngine::loadModel(const juce::File& modelFile) {
    if (!modelFile.existsAsFile()) return false;

    // TODO: 这里未来接入你真实的 RTNeural 读取代码
    // 例如: myModel.parseJson(modelFile.loadFileAsString().toStdString());

    currentModelName = modelFile.getFileName();
    modelReady.store(true);
    DBG("[LK] Model loaded: " << currentModelName);
    return true;
}

void RTNeuralEngine::processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                                   std::vector<MidiEventLite>& outputPhrase,
                                   const Chord& currentChord,
                                   juce::int64 responseStartSample,
                                   juce::int64 responseLengthSamples,
                                   double sampleRate,
                                   double bpm) {
    juce::ignoreUnused(responseLengthSamples, sampleRate, bpm);
    if (!modelReady.load()) return; // 防御：模型没加载就直接返回

    std::vector<float> inputFeatures = preprocessData(inputPhrase, currentChord);
    std::vector<float> modelOutput = runModelInference(inputFeatures);
    outputPhrase = postprocessData(modelOutput, responseStartSample);
}

std::vector<float> RTNeuralEngine::preprocessData(const std::vector<MidiEventLite>& input, const Chord& chord) {
    // 🌟 加了 ignoreUnused，帮你把那些烦人的警告全部消除
    juce::ignoreUnused(input, chord);
    return std::vector<float>();
}

std::vector<float> RTNeuralEngine::runModelInference(const std::vector<float>& inputFeatures) {
    juce::ignoreUnused(inputFeatures);
    return std::vector<float>();
}

std::vector<MidiEventLite> RTNeuralEngine::postprocessData(const std::vector<float>& modelOutput, juce::int64 startSample) {
    juce::ignoreUnused(modelOutput, startSample);
    return std::vector<MidiEventLite>(); 
}