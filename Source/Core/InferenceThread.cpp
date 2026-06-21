#include "InferenceThread.h"
#include <algorithm>
#include <limits>

InferenceThread::InferenceThread(LockFreeQueue<MidiEventLite>& outQueue)
    : Thread("LK_Jam_Inference_Thread"), playbackQueue(outQueue)
{
    // 预分配内存，以后每轮 clear() 后 push_back 也不会触发 malloc
    localInputBuffer.reserve(4096);
    localOutputBuffer.reserve(4096);
}

InferenceThread::~InferenceThread() {
    signalThreadShouldExit();
    triggerEvent.signal();
    waitForThreadToExit(2000);
}

void InferenceThread::setEngine(std::unique_ptr<IInferenceEngine> engine) {
    inferenceEngine = std::move(engine);
}

void InferenceThread::clearQueues() {
    inputQueue.clear();
}

void InferenceThread::submitInputPhrase(const std::vector<MidiEventLite>& input,
                                        const Chord& chord,
                                        juce::int64 startSample,
                                        juce::int64 responseLengthSamples,
                                        double sampleRate,
                                        double bpm) {
    if (isProcessing.load()) return; // 若上一轮 AI 没算完，丢弃当前投递（可根据产品逻辑决定是否排队）

    // 1. 将音频线程录制的 Vector 无锁压入 Queue
    inputQueue.clear();
    for (const auto& ev : input) {
        inputQueue.enqueue(ev);
    }

    currentChordCtx = chord;
    currentStartSample = startSample;
    currentResponseLengthSamples = responseLengthSamples;
    currentSampleRate = sampleRate;
    currentBpm = bpm;

    currentTaskMode = TaskMode::Model;

    // 2. 唤醒工作线程
    triggerEvent.signal();
}

void InferenceThread::submitFallbackPhrase(const std::vector<MidiEventLite>& input,
                                           const Chord& chord,
                                           juce::int64 startSample,
                                           juce::int64 responseLengthSamples,
                                           double sampleRate,
                                           double bpm) {
    if (isProcessing.load()) return;

    inputQueue.clear();
    for (const auto& ev : input) {
        inputQueue.enqueue(ev);
    }

    currentChordCtx = chord;
    currentStartSample = startSample;
    currentResponseLengthSamples = responseLengthSamples;
    currentSampleRate = sampleRate;
    currentBpm = bpm;
    currentTaskMode = TaskMode::Fallback;

    triggerEvent.signal();
}

static void generateFallbackPhrase(const std::vector<MidiEventLite>& input,
                                   std::vector<MidiEventLite>& output,
                                   const Chord& chord,
                                   juce::int64 responseLengthSamples,
                                   double sampleRate,
                                   double bpm) {
    output.clear();
    if (responseLengthSamples <= 0 || sampleRate <= 0.0 || bpm <= 0.0) return;

    const double samplesPerBeat = (sampleRate * 60.0) / bpm;

    struct FallbackNote {
        juce::int64 start = 0;
        juce::int64 duration = 0;
        int pitch = 60;
        int velocity = 80;
    };

    std::vector<FallbackNote> notes;
    std::array<juce::int64, 128> noteStarts;
    std::array<int, 128> noteVelocities;
    std::array<bool, 128> noteIsOn;
    noteStarts.fill(0);
    noteVelocities.fill(80);
    noteIsOn.fill(false);

    juce::int64 phraseStartSample = std::numeric_limits<juce::int64>::max();
    for (const auto& e : input) {
        if (e.isNoteOn) {
            phraseStartSample = std::min<juce::int64>(phraseStartSample, e.sampleOffset);
        }
    }

    if (phraseStartSample != std::numeric_limits<juce::int64>::max()) {
        for (const auto& e : input) {
            if (e.pitch < 0 || e.pitch >= 128) continue;

            if (e.isNoteOn) {
                noteStarts[static_cast<size_t>(e.pitch)] = e.sampleOffset - phraseStartSample;
                noteVelocities[static_cast<size_t>(e.pitch)] = e.velocity;
                noteIsOn[static_cast<size_t>(e.pitch)] = true;
            } else if (noteIsOn[static_cast<size_t>(e.pitch)]) {
                auto start = noteStarts[static_cast<size_t>(e.pitch)];
                const auto minFallbackNoteSamples = static_cast<juce::int64>(std::max(1.0, samplesPerBeat * 0.125));
                auto duration = std::max<juce::int64>(minFallbackNoteSamples, e.sampleOffset - phraseStartSample - start);
                notes.push_back({start, duration, e.pitch, noteVelocities[static_cast<size_t>(e.pitch)]});
                noteIsOn[static_cast<size_t>(e.pitch)] = false;
            }
        }

        const auto defaultDuration = static_cast<juce::int64>(std::max(1.0, samplesPerBeat * 0.5));
        for (int pitch = 0; pitch < 128; ++pitch) {
            if (noteIsOn[static_cast<size_t>(pitch)]) {
                notes.push_back({noteStarts[static_cast<size_t>(pitch)], defaultDuration, pitch, noteVelocities[static_cast<size_t>(pitch)]});
            }
        }
    }

    if (!notes.empty()) {
        std::sort(notes.begin(), notes.end(), [](const FallbackNote& a, const FallbackNote& b) { return a.start < b.start; });
        const auto minFallbackPhraseSamples = static_cast<juce::int64>(std::max(1.0, samplesPerBeat));
        const auto phraseLengthSamples = std::max<juce::int64>(minFallbackPhraseSamples, notes.back().start + notes.back().duration);

        int emittedNoteIndex = 0;
        const int maxFallbackNotes = 512;
        for (juce::int64 base = 0; base < responseLengthSamples && emittedNoteIndex < maxFallbackNotes; base += phraseLengthSamples) {
            for (const auto& note : notes) {
                if (emittedNoteIndex >= maxFallbackNotes) break;
                auto start = base + note.start;
                if (start >= responseLengthSamples) break;

                int pitch = note.pitch;
                if (emittedNoteIndex % 8 == 4) pitch += 2;
                else if (emittedNoteIndex % 8 == 7) pitch -= 2;
                else if (emittedNoteIndex % 16 == 13) pitch += 7;
                pitch = std::clamp(pitch, 0, 127);

                auto noteOff = std::min(start + note.duration, responseLengthSamples);
                output.push_back({static_cast<int>(start), pitch, std::clamp(note.velocity, 1, 127), true});
                output.push_back({static_cast<int>(noteOff), pitch, 0, false});
                ++emittedNoteIndex;
            }
        }
        return;
    }

    if (!chord.isEmpty()) {
        const auto intervals = chord.getIntervals();
        const int baseMidi = 60 + chord.rootMidi;
        const auto noteSamples = static_cast<juce::int64>(std::max(1.0, samplesPerBeat));
        int emittedNoteIndex = 0;
        const int maxFallbackNotes = 512;

        for (juce::int64 base = 0; base < responseLengthSamples && emittedNoteIndex < maxFallbackNotes; base += noteSamples) {
            for (int interval : intervals) {
                if (emittedNoteIndex >= maxFallbackNotes) break;
                int pitch = std::clamp(baseMidi + interval, 0, 127);
                auto off = std::min<juce::int64>(base + noteSamples, responseLengthSamples);
                output.push_back({static_cast<int>(base), pitch, 80, true});
                output.push_back({static_cast<int>(off), pitch, 0, false});
                ++emittedNoteIndex;
            }
        }
    }
}

void InferenceThread::run() {
    while (!threadShouldExit()) {

        triggerEvent.wait();
        if (threadShouldExit()) break;

        isProcessing.store(true);

        // ✅ 将无锁队列中的数据倒入本地缓存
        localInputBuffer.clear();
        MidiEventLite ev;
        while (inputQueue.dequeue(ev)) {
            localInputBuffer.push_back(ev);
        }

        if (currentTaskMode == TaskMode::Fallback) {
            localOutputBuffer.clear();
            generateFallbackPhrase(localInputBuffer,
                                   localOutputBuffer,
                                   currentChordCtx,
                                   currentResponseLengthSamples,
                                   currentSampleRate,
                                   currentBpm);

            DBG("[LK] InferenceThread fallback: input=" << localInputBuffer.size()
                << " output=" << localOutputBuffer.size()
                << " lengthSamples=" << currentResponseLengthSamples);

            for (const auto& outEv : localOutputBuffer) {
                playbackQueue.enqueue(outEv);
            }
        }
        else if (inferenceEngine != nullptr && !localInputBuffer.empty()) {
            localOutputBuffer.clear();

            inferenceEngine->processPhrase(localInputBuffer,
                                           localOutputBuffer,
                                           currentChordCtx,
                                           currentStartSample,
                                           currentResponseLengthSamples,
                                           currentSampleRate,
                                           currentBpm);

            DBG("[LK] InferenceThread: input=" << localInputBuffer.size()
                << " output=" << localOutputBuffer.size()
                << " startSample=" << currentStartSample
                << " lengthSamples=" << currentResponseLengthSamples);

            for (const auto& outEv : localOutputBuffer) {
                playbackQueue.enqueue(outEv);
            }
        } else {
            DBG("[LK] InferenceThread: SKIPPED engine=" << (inferenceEngine != nullptr ? 1 : 0)
                << " inputSize=" << (int)localInputBuffer.size());
        }

        isProcessing.store(false);
    }
}