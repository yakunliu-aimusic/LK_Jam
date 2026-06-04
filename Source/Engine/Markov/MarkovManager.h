/*
==============================================================================

    MarkovManager.h
    Created: 30 Oct 2019 3:28:02pm
    Author:  matthew

  ==============================================================================
*/

#pragma once
#include "MarkovChain.h"
#include <mutex>

/**
 * Manages a markov chain for training and generation purposes
 */
class MarkovManager {
public:
    MarkovManager(unsigned long maxOrder=100, unsigned long chainEventMemoryLength=20);
    ~MarkovManager();

    void putEvent(state_single symbol);
    state_single getEvent(bool needChoices = true, bool useInputAsContext = false);
    int getOrderOfLastEvent();
    void observeContextOnly(state_single symbol);
    void reset();

    void addStateToStateSequence(state_sequence& seq, state_single new_state);
    void giveNegativeFeedback();
    void givePositiveFeedback();

    bool saveModel(const std::string& filename);
    bool loadModel(const std::string& filename);
    bool saveModelBinary(const std::string& filename);
    bool loadModelBinary(const std::string& filename);

    std::string getModelAsString();
    std::string getModelAsBinaryString();
    bool setupModelFromString(const std::string&);
    bool setupModelFromBinaryString(const std::string&);

    MarkovChain getCopyOfModel();
    size_t getModelSize();
    int getLastOrderOfMatch();
    void setMaxSameOrderRepeats(unsigned int maxRepeats);

private:
    void rememberChainEvent(state_and_observation event);
    void resetGenerationMemory();

    state_sequence inputMemory;
    state_sequence outputMemory;
    MarkovChain chain;
    std::vector<state_and_observation> chainEvents;
    unsigned long  maxChainEventMemory;
    unsigned long  chainEventIndex;
    std::mutex mtx;
    int lastGeneratedOrder { -1 };
    unsigned int sameOrderRepeatCount { 0 };
    unsigned int maxSameOrderRepeats { 10 };
};