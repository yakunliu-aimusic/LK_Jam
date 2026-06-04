#include "InferenceThread.h"

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

void InferenceThread::submitInputPhrase(const std::vector<MidiEventLite>& input, const Chord& chord, juce::int64 startSample) {
    if (isProcessing.load()) return; // 若上一轮 AI 没算完，丢弃当前投递（可根据产品逻辑决定是否排队）

    // 1. 将音频线程录制的 Vector 无锁压入 Queue
    inputQueue.clear();
    for (const auto& ev : input) {
        inputQueue.enqueue(ev);
    }

    currentChordCtx = chord;
    currentStartSample = startSample;

    // 2. 唤醒工作线程
    triggerEvent.signal();
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

        if (inferenceEngine != nullptr && !localInputBuffer.empty()) {
            localOutputBuffer.clear();

            // 执行重度 AI 推理 (RTNeural 代码运行在这里)
            inferenceEngine->processPhrase(localInputBuffer, localOutputBuffer, currentChordCtx, currentStartSample);

            // 推理完成，安全推入无锁队列给 Audio 线程回放
            for (const auto& outEv : localOutputBuffer) {
                playbackQueue.enqueue(outEv);
            }
        }

        isProcessing.store(false);
    }
}