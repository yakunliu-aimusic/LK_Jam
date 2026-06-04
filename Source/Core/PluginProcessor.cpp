#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Engine/MarkovEngine.h"
#include <cmath>

LK_Jam_POCProcessor::LK_Jam_POCProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      sessionDirector(stateMachine, syncEngine),
      inferenceThread(playbackQueue)
{
    apvts.state = juce::ValueTree("LK_Jam_State");

    turnBarsParam = apvts.getRawParameterValue("turnBars");
    cycleBarsParam = apvts.getRawParameterValue("cycleBars");
    modelChoiceParam = apvts.getRawParameterValue("modelChoice");
    fallbackModeParam = apvts.getRawParameterValue("fallbackMode");

    inferenceThread.setEngine(std::make_unique<MarkovEngine>());
    inferenceThread.startThread();
}

LK_Jam_POCProcessor::~LK_Jam_POCProcessor() {
    inferenceThread.stopThread(1000);
}

juce::AudioProcessorValueTreeState::ParameterLayout LK_Jam_POCProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("temperature", 1), "Temperature", 0.1f, 2.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("topP", 1), "Top-P", 0.1f, 1.0f, 0.9f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("modelChoice", 1), "Model",
                                                           juce::StringArray{"None (Fallback Only)", "Markov Chain Engine", "GRU Neural Net (WIP)"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("styleChoice", 1), "Style",
                                                           juce::StringArray{"Jazz", "Pop", "Experimental"}, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("turnBars", 1), "Turn Bars", 1, 16, 4));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID("cycleBars", 1), "Cycle Bars", 2, 32, 8));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("fallbackMode", 1), "Fallback Mode", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("aiTrigger", 1), "AI Trigger (Auto)", 0.0f, 1.0f, 0.0f));

    return layout;
}

void LK_Jam_POCProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    (void)samplesPerBlock;
    currentSampleRate = sampleRate;
    captureBuffer.clear();
    playbackQueue.clear();
    inferenceThread.clearQueues();
    activeNotes.fill(false);
    currentAbsoluteSample = 0;
    testTonePhase = 0.0f;
    uiToAiCount.store(0);
    uiFromAiCount.store(0);
}

void LK_Jam_POCProcessor::resetLoopState() {
    shouldResetClock.store(true);
    currentAbsoluteSample = 0;
    syncEngine.requestReset(); // 🌟 通知同步引擎更新相对时间锚点
    playbackQueue.clear();
    inferenceThread.clearQueues();
    captureBuffer.clear();
}

void LK_Jam_POCProcessor::releaseResources() {}

bool LK_Jam_POCProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    juce::ignoreUnused(layouts);
    return true;
}

void LK_Jam_POCProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto startTicks = juce::Time::getHighResolutionTicks();

    // ==========================================
    // 拦截区 A：UI 触发的软复位逻辑 (RESET)
    // ==========================================
    if (shouldResetClock.exchange(false)) {
        playbackQueue.clear();
        inferenceThread.clearQueues();
        captureBuffer.clear();
        midiMessages.clear();
        syncEngine.requestReset(); // 🌟 确保引擎得到重置指令

        for (int i = 0; i < 128; ++i) {
            if (activeNotes[static_cast<size_t>(i)] || fallbackActiveNotes[static_cast<size_t>(i)]) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, i), 0);
            }
        }
        activeNotes.fill(false);
        fallbackActiveNotes.fill(false);
        for (int i = 1; i <= 16; ++i) midiMessages.addEvent(juce::MidiMessage::allNotesOff(i), 0);

        stateMachine.setState(InteractionState::Idle);
        cachedNextEvent.reset();
    }

    // ==========================================
    // 拦截区 B：Panic 紧急避险硬断音
    // ==========================================
    if (panicTriggered.exchange(false)) {
        playbackQueue.clear();
        inferenceThread.clearQueues();
        captureBuffer.clear();
        midiMessages.clear();

        for (int i = 0; i < 128; ++i) {
            if (activeNotes[static_cast<size_t>(i)] || fallbackActiveNotes[static_cast<size_t>(i)]) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, i), 0);
            }
        }
        activeNotes.fill(false);
        fallbackActiveNotes.fill(false);
        for (int i = 1; i <= 16; ++i) midiMessages.addEvent(juce::MidiMessage::allNotesOff(i), 0);

        cachedNextEvent.reset();
        buffer.clear();
        return;
    }

    if (isTestToneEnabled.load()) {
        midiMessages.clear();
        float phaseDelta = (1000.0f * juce::MathConstants<float>::twoPi) / static_cast<float>(currentSampleRate);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            float out = std::sin(testTonePhase) * 0.1f;
            buffer.setSample(0, sample, out);
            buffer.setSample(1, sample, out);
            testTonePhase += phaseDelta;
            if (testTonePhase > juce::MathConstants<float>::twoPi) testTonePhase -= juce::MathConstants<float>::twoPi;
        }
        currentAbsoluteSample += buffer.getNumSamples();
        return;
    }
    buffer.clear();

    auto* playHead = getPlayHead();

    // ==========================================
    // 基础参数解析与时钟更新 (已彻底删除错误的回合防越界限制)
    // ==========================================
    const auto& currentProg = getActiveProgression();
    int currentTurnBars = turnBarsParam ? static_cast<int>(turnBarsParam->load()) : 4;
    int currentCycleBars = static_cast<int>(currentProg.measures.size());
    int modelChoice = modelChoiceParam ? static_cast<int>(modelChoiceParam->load()) : 0;
    bool fallbackEnabled = fallbackModeParam ? (fallbackModeParam->load() > 0.5f) : true;

    syncEngine.update(playHead, stateMachine, currentTurnBars, currentCycleBars, currentSampleRate, buffer.getNumSamples());

    double currentPpq = syncEngine.getCurrentPpq();
    double totalBeats = std::max(1.0, currentProg.getTotalBeats());

    // ==========================================
    // 1. 交由 SessionDirector 推动状态机流转
    // ==========================================
    bool uiForceLearning = isLearning.load();
    sessionDirector.process(apvts, midiMessages, uiForceLearning);

    isHostSynced.store(syncEngine.isUsingHostClock());
    currentBpm.store(syncEngine.getCurrentBpm());

    juce::int64 currentBlockStartSample = currentAbsoluteSample;
    if (playHead && playHead->getPosition() && playHead->getPosition()->getTimeInSamples().hasValue()) {
        currentBlockStartSample = *playHead->getPosition()->getTimeInSamples();
    }

    // ==========================================
    // 2. 状态变迁处理
    // ==========================================
    InteractionState currentState = stateMachine.getState();
    bool loopExpired = (currentProg.totalLoops > 0 && currentPpq >= (totalBeats * currentProg.totalLoops));

    if (loopExpired) {
        playbackQueue.clear();
        for (int i = 0; i < 128; ++i) {
            if (fallbackActiveNotes[static_cast<size_t>(i)]) {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, i), 0);
                fallbackActiveNotes[static_cast<size_t>(i)] = false;
            }
        }
        currentState = InteractionState::Idle;
    }

    if (syncEngine.isStateChangedThisBlock() && !loopExpired) {
        if (currentState == InteractionState::Responding) {
            double currentPpqInLoop = std::fmod(syncEngine.getCurrentPpq(), totalBeats);

            Chord mockChord;
            mockChord.rootMidi = 0;
            mockChord.quality = ChordQuality::Major;

            if (!currentProg.measures.empty()) {
                double accumulatedBeats = 0.0;
                for (const auto& m : currentProg.measures) {
                    accumulatedBeats += currentProg.getBeatsPerMeasure();
                    if (currentPpqInLoop < accumulatedBeats) {
                        if (!m.chord.isEmpty()) mockChord = m.chord;
                        break;
                    }
                }
            }

            if (isAiPlaying.load() && modelChoice > 0) {
                auto events = captureBuffer.getRecordedEvents();
                inferenceThread.submitInputPhrase(events, mockChord, currentBlockStartSample);

                int tCount = 0;
                for (const auto& e : events) {
                    if (e.isNoteOn && tCount < 16) {
                        uiToAiPitches[tCount].store(e.pitch);
                        tCount++;
                    }
                }
                uiToAiCount.store(tCount);
                uiFromAiCount.store(0);
            }
            else if (fallbackEnabled) {
                if (!mockChord.isEmpty()) {
                    std::vector<int> intervals = mockChord.getIntervals();
                    int baseMidi = 60 + mockChord.rootMidi;

                    double currentBpmReal = std::max(1.0, syncEngine.getCurrentBpm());
                    double samplesPerBeat = (currentSampleRate * 60.0) / currentBpmReal;
                    double measureBeats = currentProg.getBeatsPerMeasure();
                    int noteSamples = static_cast<int>(samplesPerBeat * measureBeats);

                    for (int i = 0; i < 128; ++i) {
                        if (fallbackActiveNotes[static_cast<size_t>(i)]) {
                            midiMessages.addEvent(juce::MidiMessage::noteOff(1, i), 0);
                            fallbackActiveNotes[static_cast<size_t>(i)] = false;
                        }
                    }

                    for (int interval : intervals) {
                        int pitch = std::clamp(baseMidi + interval, 0, 127);
                        playbackQueue.enqueue({(int)currentBlockStartSample, pitch, 80, true});
                        playbackQueue.enqueue({(int)(currentBlockStartSample + noteSamples), pitch, 0, false});

                        fallbackActiveNotes[static_cast<size_t>(pitch)] = true;
                    }
                }
            }

            captureBuffer.clear();
            activeNotes.fill(false);
        }
        else {
            playbackQueue.clear();
            inferenceThread.clearQueues();
            captureBuffer.clear();
            activeNotes.fill(false);
            cachedNextEvent.reset();
        }
    }

    if (currentState == InteractionState::Listening) {
        for (const auto metadata : midiMessages) {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn() || msg.isNoteOff()) {
                int noteNum = msg.getNoteNumber();
                juce::int64 absTime = currentBlockStartSample + metadata.samplePosition;

                if (msg.isNoteOn()) {
                    if (noteNum >= 0 && noteNum < 128) activeNotes[static_cast<size_t>(noteNum)] = true;
                    captureBuffer.addEvent({static_cast<int>(absTime), noteNum, msg.getVelocity(), true});
                } else if (msg.isNoteOff()) {
                    if (noteNum >= 0 && noteNum < 128) activeNotes[static_cast<size_t>(noteNum)] = false;
                    captureBuffer.addEvent({static_cast<int>(absTime), noteNum, 0, false});
                }
            }
        }
    }
    else if (currentState == InteractionState::Responding && isAiPlaying.load()) {
        midiMessages.clear();
        while (true) {
            MidiEventLite eventToCheck;
            if (cachedNextEvent.has_value()) {
                eventToCheck = cachedNextEvent.value();
            } else if (!playbackQueue.dequeue(eventToCheck)) {
                break;
            }

            if (eventToCheck.isNoteOn) {
                int fCount = uiFromAiCount.load();
                if (fCount < 16) {
                    uiFromAiPitches[fCount].store(eventToCheck.pitch);
                    uiFromAiCount.store(fCount + 1);
                } else {
                    for(int i = 1; i < 16; ++i) {
                        uiFromAiPitches[i - 1].store(uiFromAiPitches[i].load());
                    }
                    uiFromAiPitches[15].store(eventToCheck.pitch);
                }
            }

            if (eventToCheck.sampleOffset < currentBlockStartSample) {
                cachedNextEvent.reset();
                juce::MidiMessage msg = eventToCheck.isNoteOn ? juce::MidiMessage::noteOn(1, eventToCheck.pitch, (juce::uint8)eventToCheck.velocity) : juce::MidiMessage::noteOff(1, eventToCheck.pitch);
                midiMessages.addEvent(msg, 0);
            }
            else if (eventToCheck.sampleOffset < currentBlockStartSample + buffer.getNumSamples()) {
                cachedNextEvent.reset();
                int relativeOffset = static_cast<int>(eventToCheck.sampleOffset - currentBlockStartSample);
                juce::MidiMessage msg = eventToCheck.isNoteOn ? juce::MidiMessage::noteOn(1, eventToCheck.pitch, (juce::uint8)eventToCheck.velocity) : juce::MidiMessage::noteOff(1, eventToCheck.pitch);
                midiMessages.addEvent(msg, relativeOffset);
            } else {
                cachedNextEvent = eventToCheck;
                break;
            }
        }
    }

    bool isHostPlaying = false;
    if (playHead && playHead->getPosition()) {
        isHostPlaying = playHead->getPosition()->getIsPlaying();
    }

    if (isMetronomeEnabled.load() && isHostPlaying && currentState != InteractionState::Idle) {
        int currentBeatInt = static_cast<int>(currentPpq);

        if (currentBeatInt != lastMetronomeBeat) {
            lastMetronomeBeat = currentBeatInt;
            metronomeEnvelope = 1.0f;
        }

        if (metronomeEnvelope > 0.0f) {
            float freq = (currentBeatInt % 4 == 0) ? 1500.0f : 800.0f;
            float phaseDelta = (freq * juce::MathConstants<float>::twoPi) / static_cast<float>(currentSampleRate);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                float clickSample = std::sin(metronomePhase) * metronomeEnvelope * 0.3f;
                buffer.addSample(0, sample, clickSample);
                buffer.addSample(1, sample, clickSample);
                metronomePhase += phaseDelta;
                if (metronomePhase > juce::MathConstants<float>::twoPi)
                    metronomePhase -= juce::MathConstants<float>::twoPi;
                metronomeEnvelope *= 0.998f;
                if (metronomeEnvelope < 0.001f) metronomeEnvelope = 0.0f;
            }
        }
    }

    currentAbsoluteSample += buffer.getNumSamples();
    auto endTicks = juce::Time::getHighResolutionTicks();
    double elapsedMs = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks) * 1000.0;
    currentLatencyMs.store(elapsedMs);
    currentCpuUsage.store((elapsedMs / (buffer.getNumSamples() / currentSampleRate * 1000.0)) * 100.0);
}

juce::AudioProcessorEditor* LK_Jam_POCProcessor::createEditor() { return new LK_Jam_POCEditor(*this); }
void LK_Jam_POCProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) copyXmlToBinary(*xml, destData);
}
void LK_Jam_POCProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LK_Jam_POCProcessor();
}