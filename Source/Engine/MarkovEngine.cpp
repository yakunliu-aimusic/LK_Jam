#include "MarkovEngine.h"
#include <algorithm>
#include <cmath>

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

void MarkovEngine::resetModelMemory() {
    markovManager.reset();
    modelReady = false;
}

// 将 velocity 量化到 4 个档位 (0-3)
static int quantizeVelocity(int vel) {
    if (vel < 32)  return 0;
    if (vel < 64)  return 1;
    if (vel < 96)  return 2;
    return 3;
}

// 将时值（单位：拍）量化到 5 个档位 (0-4)
static int quantizeDuration(double beats) {
    if (beats < 0.25) return 0;
    if (beats < 0.5)  return 1;
    if (beats < 1.0)  return 2;
    if (beats < 2.0)  return 3;
    return 4;
}

// 将复合 token 解码回 pitch / velocity / durationBucket
static bool decodeToken(const std::string& token, int& pitch, int& velBucket, int& durBucket) {
    auto sep1 = token.find(':');
    if (sep1 == std::string::npos) return false;
    auto sep2 = token.find(':', sep1 + 1);
    if (sep2 == std::string::npos) return false;

    try {
        pitch     = std::stoi(token.substr(0, sep1));
        velBucket = std::stoi(token.substr(sep1 + 1, sep2 - sep1 - 1));
        durBucket = std::stoi(token.substr(sep2 + 1));
    } catch (...) {
        return false;
    }
    return true;
}

// durationBucket → 代表性时值（拍），用于生成时的 noteDuration
static double durationBucketToBeats(int bucket) {
    switch (bucket) {
        case 0: return 0.15;
        case 1: return 0.375;
        case 2: return 0.75;
        case 3: return 1.5;
        default: return 2.5;
    }
}

// velBucket → 代表性 MIDI velocity
static int velBucketToVelocity(int bucket) {
    switch (bucket) {
        case 0: return 24;
        case 1: return 48;
        case 2: return 80;
        default: return 110;
    }
}

void MarkovEngine::processPhrase(const std::vector<MidiEventLite>& inputPhrase,
                                 std::vector<MidiEventLite>& outputPhrase,
                                 const Chord& currentChord,
                                 juce::int64 responseStartSample,
                                 juce::int64 responseLengthSamples,
                                 double sampleRate,
                                 double bpm) {
    juce::ignoreUnused(currentChord);

    if (sampleRate <= 0.0 || bpm <= 0.0) return;

    const double samplesPerBeat = (sampleRate * 60.0) / bpm;

    // 配对 noteOn/noteOff，计算每个音符的时值，编码成复合 token 训练
    std::map<int, juce::int64> noteOnTimes;   // pitch -> noteOn sampleOffset
    std::map<int, int>         noteOnVel;     // pitch -> velocity

    for (const auto& ev : inputPhrase) {
        if (ev.isNoteOn) {
            noteOnTimes[ev.pitch] = ev.sampleOffset;
            noteOnVel[ev.pitch]   = ev.velocity;
        } else {
            auto it = noteOnTimes.find(ev.pitch);
            if (it != noteOnTimes.end()) {
                const double durationSamples = static_cast<double>(ev.sampleOffset - it->second);
                const double durationBeats   = durationSamples / samplesPerBeat;
                const int    velBucket       = quantizeVelocity(noteOnVel[ev.pitch]);
                const int    durBucket       = quantizeDuration(durationBeats);

                const std::string token = std::to_string(ev.pitch) + ":"
                                        + std::to_string(velBucket) + ":"
                                        + std::to_string(durBucket);
                markovManager.putEvent(token);
                noteOnTimes.erase(it);
                noteOnVel.erase(ev.pitch);
            }
        }
    }

    // 未配对到 noteOff 的音符（还按着没松），用默认四分音符时值训练
    for (const auto& [pitch, onSample] : noteOnTimes) {
        const int velBucket = quantizeVelocity(noteOnVel[pitch]);
        const std::string token = std::to_string(pitch) + ":"
                                + std::to_string(velBucket) + ":2";
        markovManager.putEvent(token);
    }

    if (responseLengthSamples <= 0) return;

    const juce::int64 responseEndSample = responseStartSample + responseLengthSamples;
    juce::int64 currentSample = responseStartSample;
    int consecutiveZeros = 0;
    const int maxConsecutiveZeros = 3;

    while (currentSample < responseEndSample) {
        std::string token = markovManager.getEvent(false, false);

        if (token == "0" || token.empty()) {
            consecutiveZeros++;
            if (consecutiveZeros >= maxConsecutiveZeros) {
                token = markovManager.getEvent(false, true);
                consecutiveZeros = 0;
            }
        } else {
            consecutiveZeros = 0;
        }

        int pitch = -1, velBucket = 2, durBucket = 2;
        if (token != "0" && !token.empty() && decodeToken(token, pitch, velBucket, durBucket)) {
            pitch = std::clamp(pitch, 0, 127);
            const int    velocity        = velBucketToVelocity(velBucket);
            const double noteBeats       = durationBucketToBeats(durBucket);
            const auto   noteDurSamples  = static_cast<juce::int64>(std::max(1.0, noteBeats * samplesPerBeat * 0.9));
            const juce::int64 noteOffTime = std::min(currentSample + noteDurSamples, responseEndSample);

            outputPhrase.push_back({static_cast<int>(currentSample),   pitch, velocity, true});
            outputPhrase.push_back({static_cast<int>(noteOffTime),      pitch, 0,        false});

            const auto stepSamples = static_cast<juce::int64>(std::max(1.0, noteBeats * samplesPerBeat));
            currentSample += stepSamples;
        } else {
            currentSample += static_cast<juce::int64>(std::max(1.0, samplesPerBeat * 0.5));
        }
    }

    // Fallback：chain 完全空时，直接 echo 输入（保留原始时值和力度）
    if (outputPhrase.empty() && !inputPhrase.empty()) {
        currentSample = responseStartSample;
        for (const auto& ev : inputPhrase) {
            if (!ev.isNoteOn || currentSample >= responseEndSample) continue;
            const auto stepSamples = static_cast<juce::int64>(std::max(1.0, samplesPerBeat * 0.5));
            const auto noteOffTime = std::min(currentSample + stepSamples - 1, responseEndSample);
            outputPhrase.push_back({static_cast<int>(currentSample), ev.pitch, ev.velocity, true});
            outputPhrase.push_back({static_cast<int>(noteOffTime),   ev.pitch, 0, false});
            currentSample += stepSamples;
        }
    }
}