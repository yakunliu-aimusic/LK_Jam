#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Engine/MarkovEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

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
    aiTriggerParam = apvts.getRawParameterValue("aiTrigger");

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
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("aiTrigger", 1), "AI Arm / Run", 0.0f, 1.0f, 0.0f));

    return layout;
}

void LK_Jam_POCProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    (void)samplesPerBlock;
    currentSampleRate = sampleRate;
    captureBuffer.clear();
    playbackQueue.clear();
    inferenceThread.clearQueues();
    activeNotes.fill(false);
    aiActiveNotes.fill(false);
    fallbackActiveNotes.fill(false);
    currentAbsoluteSample = 0;
    testTonePhase = 0.0f;
    metronomePhase = 0.0f;
    metronomeEnvelope = 0.0f;
    lastMetronomeBeat = -1;
    uiToAiCount.store(0);
    uiFromAiCount.store(0);
    isClockRunning.store(false);
    lastProcessedState = InteractionState::Idle;
    responseAnchorSet = false;
    responseAnchorSample = 0;
}

bool LK_Jam_POCProcessor::shouldTransportRun() const {
    const int syncMode = syncEngine.getSyncMode();
    if (syncMode == 1) {
        if (auto* playHead = const_cast<LK_Jam_POCProcessor*>(this)->getPlayHead()) {
            if (playHead->getPosition()) {
                const bool hostPlaying = playHead->getPosition()->getIsPlaying();
                const bool triggerArmed = aiTriggerParam != nullptr && aiTriggerParam->load() > 0.5f;
                return hostPlaying && triggerArmed;
            }
        }
        return false;
    }

    if (syncMode == 2) {
        return isTransportRunning.load();
    }

    return isTransportRunning.load();
}

void LK_Jam_POCProcessor::flushAllActiveNotes(juce::MidiBuffer& midiMessages, int sampleOffset) {
    for (int channel = 1; channel <= 16; ++channel) {
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(channel, 64, 0), sampleOffset);
        midiMessages.addEvent(juce::MidiMessage::allNotesOff(channel), sampleOffset);
        midiMessages.addEvent(juce::MidiMessage::allSoundOff(channel), sampleOffset);

        for (int note = 0; note < 128; ++note) {
            midiMessages.addEvent(juce::MidiMessage::noteOff(channel, note), sampleOffset);
        }
    }

    activeNotes.fill(false);
    aiActiveNotes.fill(false);
    fallbackActiveNotes.fill(false);
}

void LK_Jam_POCProcessor::flushPerformanceActiveNotes(juce::MidiBuffer& midiMessages, int sampleOffset) {
    for (int i = 0; i < 128; ++i) {
        const auto index = static_cast<size_t>(i);
        if (activeNotes[index] || aiActiveNotes[index]) {
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, i), sampleOffset);
        }
        activeNotes[index] = false;
        aiActiveNotes[index] = false;
    }
}

void LK_Jam_POCProcessor::clearRuntimeQueues() {
    playbackQueue.clear();
    inferenceThread.clearQueues();
    captureBuffer.clear();
    cachedNextEvent.reset();
    responseAnchorSet = false;
}

void LK_Jam_POCProcessor::resetLoopState(bool clearModelMemory) {
    shouldResetClock.store(true);
    if (clearModelMemory) {
        shouldResetModelMemory.store(true);
    }
    currentAbsoluteSample = 0;
    syncEngine.requestReset();
    syncEngine.setOverrideMode(false);
    clearRuntimeQueues();
    metronomePhase = 0.0f;
    metronomeEnvelope = 0.0f;
    lastMetronomeBeat = -1;
    lastProcessedState = InteractionState::Idle;
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
        midiMessages.clear();
        if (shouldResetModelMemory.exchange(false)) {
            inferenceThread.clearQueues();
            inferenceThread.resetModelMemory();
        }
        syncEngine.requestReset();
        syncEngine.setOverrideMode(false);
        clearRuntimeQueues();
        flushAllActiveNotes(midiMessages);
        stateMachine.setState(InteractionState::Idle);
        isClockRunning.store(false);
        lastProcessedState = InteractionState::Idle;
    }

    // ==========================================
    // 拦截区 B：Panic 紧急避险硬断音
    // ==========================================
    if (panicTriggered.exchange(false)) {
        midiMessages.clear();
        clearRuntimeQueues();
        if (panicShouldResetModelMemory.exchange(false)) {
            inferenceThread.resetModelMemory();
        }
        syncEngine.setOverrideMode(false);
        flushAllActiveNotes(midiMessages);
        stateMachine.setState(InteractionState::Idle);
        isClockRunning.store(false);
        lastProcessedState = InteractionState::Idle;
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
    int currentCycleBars = cycleBarsParam ? static_cast<int>(cycleBarsParam->load()) : 8;
    int modelChoice = modelChoiceParam ? static_cast<int>(modelChoiceParam->load()) : 0;
    bool fallbackEnabled = fallbackModeParam ? (fallbackModeParam->load() > 0.5f) : true;

    bool transportRunning = shouldTransportRun();

    if (syncEngine.getSyncMode() == 1) {
        const bool triggerActive = transportRunning;
        if (triggerActive && !lastAiTriggerState) {
            double rawHostPpq = 0.0;
            double hostPpqPerBar = 4.0;
            if (playHead != nullptr && playHead->getPosition()) {
                auto posInfo = playHead->getPosition();
                rawHostPpq = posInfo->getPpqPosition().orFallback(0.0);
                auto sig = posInfo->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{4, 4});
                hostPpqPerBar = std::max(1.0, sig.numerator * (4.0 / static_cast<double>(sig.denominator)));
            }

            syncEngine.anchorToHostPpq(rawHostPpq, hostPpqPerBar);
            stateMachine.setState(InteractionState::Listening);
            clearRuntimeQueues();
            activeNotes.fill(false);
            aiActiveNotes.fill(false);
            fallbackActiveNotes.fill(false);
            lastProcessedState = InteractionState::Idle;
        } else if (!triggerActive && lastAiTriggerState) {
            clearRuntimeQueues();
            syncEngine.disarmPreRoll();
            syncEngine.setOverrideMode(false);
            panicTriggered.store(true);
        }
        lastAiTriggerState = triggerActive;
    }

    isClockRunning.store(transportRunning);
    syncEngine.update(playHead, stateMachine, currentTurnBars, currentCycleBars, currentSampleRate, buffer.getNumSamples(), transportRunning);

    double currentPpq = syncEngine.getCurrentPpq();
    double ppqPerBar = syncEngine.getPpqPerBar();
    double turnBeats = currentTurnBars * ppqPerBar;
    double fullCycleBeats = turnBeats * 2.0;
    double totalBeats = std::max(1.0, fullCycleBeats);

    // ==========================================
    // 1. 交由 SessionDirector 推动状态机流转
    // ==========================================
    if (transportRunning) {
        bool uiForceLearning = isLearning.load();
        sessionDirector.process(apvts, midiMessages, uiForceLearning);
    } else {
        isLearning.store(false);
    }

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
        DBG("[LK] loopExpired! currentPpq=" << currentPpq << " threshold=" << (totalBeats * currentProg.totalLoops));
    }

    if (!transportRunning || loopExpired) {
        clearRuntimeQueues();
        syncEngine.setOverrideMode(false);
        flushAllActiveNotes(midiMessages);
        stateMachine.setState(InteractionState::Idle);
        isClockRunning.store(false);
        currentState = InteractionState::Idle;
        lastProcessedState = InteractionState::Idle;
    }

    bool stateChanged = (currentState != lastProcessedState);
    if (stateChanged) {
        DBG("[LK] STATE CHANGED -> " << static_cast<int>(currentState)
            << " ppq=" << currentPpq << " turnBars=" << currentTurnBars
            << " totalBeats=" << totalBeats << " loopExpired=" << (int)loopExpired);
        flushAllActiveNotes(midiMessages);
        lastProcessedState = currentState;
    }

    if (stateChanged && !loopExpired && transportRunning) {
        if (currentState == InteractionState::Responding) {
            double progressionBeats = std::max(1.0, currentProg.getTotalBeats());
            double currentPpqInProgression = std::fmod(syncEngine.getCurrentPpq(), progressionBeats);

            Chord mockChord;
            mockChord.rootMidi = 0;
            mockChord.quality = ChordQuality::Major;

            if (!currentProg.measures.empty()) {
                const double beatsPerMeasure = currentProg.getBeatsPerMeasure();
                int measureIndex = static_cast<int>(currentPpqInProgression / std::max(1.0, beatsPerMeasure));
                measureIndex = std::clamp(measureIndex, 0, static_cast<int>(currentProg.measures.size()) - 1);

                double ppqInMeasure = currentPpqInProgression - (measureIndex * beatsPerMeasure);
                const int chordSlots = std::max(1, static_cast<int>(std::round(beatsPerMeasure)));
                int beatIndex = std::clamp(static_cast<int>(ppqInMeasure), 0, chordSlots - 1);

                auto chord = currentProg.measures[static_cast<size_t>(measureIndex)].getChordForBeat(beatIndex, chordSlots);
                if (!chord.isEmpty()) mockChord = chord;
            }

            if (isAiPlaying.load() && modelChoice > 0) {
                auto events = captureBuffer.getRecordedEvents();
                DBG("[LK] RESPONDING: submitting " << (int)events.size() << " events to inference"
                    << " modelChoice=" << modelChoice << " anchor=" << currentBlockStartSample);
                const double currentBpmReal = std::max(1.0, syncEngine.getCurrentBpm());
                const double samplesPerBeat = (currentSampleRate * 60.0) / currentBpmReal;
                double responseBeats = turnBeats;
                const auto responseLengthSamples = static_cast<juce::int64>(std::max(1.0, responseBeats * samplesPerBeat));
                DBG("[LK] responseBeats=" << responseBeats << " responseLengthSamples=" << responseLengthSamples);

                responseAnchorSample = currentBlockStartSample;
                responseAnchorSet = true;

                inferenceThread.submitInputPhrase(events,
                                                  mockChord,
                                                  0,
                                                  responseLengthSamples,
                                                  currentSampleRate,
                                                  currentBpmReal);

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
                auto events = captureBuffer.getRecordedEvents();
                const double currentBpmReal = std::max(1.0, syncEngine.getCurrentBpm());
                const double samplesPerBeat = (currentSampleRate * 60.0) / currentBpmReal;
                const auto responseLengthSamples = static_cast<juce::int64>(std::max(1.0, turnBeats * samplesPerBeat));

                responseAnchorSample = currentBlockStartSample;
                responseAnchorSet = true;

                inferenceThread.submitFallbackPhrase(events,
                                                     mockChord,
                                                     0,
                                                     responseLengthSamples,
                                                     currentSampleRate,
                                                     currentBpmReal);

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

            captureBuffer.clear();
            activeNotes.fill(false);
        }
        else {
            playbackQueue.clear();
            inferenceThread.clearQueues();
            captureBuffer.clear();
            activeNotes.fill(false);
            cachedNextEvent.reset();
            responseAnchorSet = false;
        }
    }

    if (currentState == InteractionState::Listening && !stateChanged) {
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
        if (stateChanged) {
            flushAllActiveNotes(midiMessages);
        }
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

            juce::int64 absoluteEventTime = eventToCheck.sampleOffset + (responseAnchorSet ? responseAnchorSample : currentBlockStartSample);

            if (absoluteEventTime < currentBlockStartSample) {
                cachedNextEvent.reset();
                juce::MidiMessage msg = eventToCheck.isNoteOn ? juce::MidiMessage::noteOn(1, eventToCheck.pitch, (juce::uint8)eventToCheck.velocity) : juce::MidiMessage::noteOff(1, eventToCheck.pitch);
                midiMessages.addEvent(msg, 0);
                if (eventToCheck.pitch >= 0 && eventToCheck.pitch < 128) {
                    aiActiveNotes[static_cast<size_t>(eventToCheck.pitch)] = eventToCheck.isNoteOn;
                }
            }
            else if (absoluteEventTime < currentBlockStartSample + buffer.getNumSamples()) {
                cachedNextEvent.reset();
                int relativeOffset = static_cast<int>(absoluteEventTime - currentBlockStartSample);
                juce::MidiMessage msg = eventToCheck.isNoteOn ? juce::MidiMessage::noteOn(1, eventToCheck.pitch, (juce::uint8)eventToCheck.velocity) : juce::MidiMessage::noteOff(1, eventToCheck.pitch);
                midiMessages.addEvent(msg, relativeOffset);
                if (eventToCheck.pitch >= 0 && eventToCheck.pitch < 128) {
                    aiActiveNotes[static_cast<size_t>(eventToCheck.pitch)] = eventToCheck.isNoteOn;
                }
            } else {
                cachedNextEvent = eventToCheck;
                break;
            }
        }
    }

    if (isMetronomeEnabled.load() && transportRunning) {
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
    if (xml != nullptr) {
        const auto& prog = getActiveProgression();
        auto* progXml = new juce::XmlElement("Progression");
        progXml->setAttribute("name", prog.name);
        progXml->setAttribute("timeSigNum", prog.timeSigNum);
        progXml->setAttribute("timeSigDen", prog.timeSigDen);
        progXml->setAttribute("totalLoops", prog.totalLoops);
        progXml->setAttribute("measures", static_cast<int>(prog.measures.size()));

        const int chordSlots = std::max(1, static_cast<int>(std::round(prog.getBeatsPerMeasure())));
        for (int measureIndex = 0; measureIndex < static_cast<int>(prog.measures.size()); ++measureIndex) {
            const auto& measure = prog.measures[static_cast<size_t>(measureIndex)];
            auto* measureXml = new juce::XmlElement("Measure");
            measureXml->setAttribute("index", measureIndex);
            measureXml->setAttribute("chord", measure.chord.name);

            for (int beat = 0; beat < chordSlots; ++beat) {
                const auto chord = measure.getChordForBeat(beat, chordSlots);
                if (!chord.isEmpty()) {
                    auto* beatXml = new juce::XmlElement("Beat");
                    beatXml->setAttribute("index", beat);
                    beatXml->setAttribute("chord", chord.name);
                    measureXml->addChildElement(beatXml);
                }
            }

            progXml->addChildElement(measureXml);
        }

        xml->addChildElement(progXml);
        copyXmlToBinary(*xml, destData);
    }
}

void LK_Jam_POCProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            auto stateTree = juce::ValueTree::fromXml(*xmlState);
            apvts.replaceState(stateTree);

            if (auto* progXml = xmlState->getChildByName("Progression")) {
                const int measures = std::max(1, progXml->getIntAttribute("measures", 16));
                Progression prog(progXml->getStringAttribute("name", "Restored Song"), measures);
                prog.timeSigNum = std::max(1, progXml->getIntAttribute("timeSigNum", 4));
                prog.timeSigDen = std::max(1, progXml->getIntAttribute("timeSigDen", 4));
                prog.totalLoops = std::max(0, progXml->getIntAttribute("totalLoops", 0));
                const int chordSlots = std::max(1, static_cast<int>(std::round(prog.getBeatsPerMeasure())));

                for (auto* measureXml : progXml->getChildIterator()) {
                    if (!measureXml->hasTagName("Measure")) continue;
                    const int measureIndex = measureXml->getIntAttribute("index", -1);
                    if (measureIndex < 0 || measureIndex >= static_cast<int>(prog.measures.size())) continue;

                    auto& measure = prog.measures[static_cast<size_t>(measureIndex)];
                    measure.ensureBeatChords(chordSlots);
                    measure.chord = Chord::fromString(measureXml->getStringAttribute("chord", ""));

                    for (auto* beatXml : measureXml->getChildIterator()) {
                        if (!beatXml->hasTagName("Beat")) continue;
                        const int beatIndex = beatXml->getIntAttribute("index", -1);
                        if (beatIndex < 0 || beatIndex >= chordSlots) continue;
                        measure.beatChords[static_cast<size_t>(beatIndex)] = Chord::fromString(beatXml->getStringAttribute("chord", ""));
                    }
                }

                setActiveProgression(prog);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new LK_Jam_POCProcessor();
}