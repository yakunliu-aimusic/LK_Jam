/*
  ==============================================================================

    MarkovChain.h
    Created: 25 Oct 2019 6:47:13am
    Author:  matthew

  ==============================================================================
*/

#pragma once

#include <vector>
#include <string>
#include <map>
#include <utility> // for std::pair

// you can change the types of observations here. string makes sense right now
// as we are mostly converting string messages anyway (e.g. from OSC/network)
typedef std::string state_single;
typedef std::vector<state_single> state_sequence;
typedef std::pair<state_single, state_single> state_and_observation;

class MarkovChain
{
public:
    MarkovChain(unsigned long limitOrder = 10);
    ~MarkovChain();

    // 🌟 安全拷贝/移动声明
    MarkovChain(const MarkovChain&) = default;
    MarkovChain& operator=(const MarkovChain&) = default;
    MarkovChain(MarkovChain&&) = default;
    MarkovChain& operator=(MarkovChain&&) = default;

    /**
     * Add a transition observation to the model.
     * @param prevState: A variable length list of previous observations. The length dictates the order.
     * @param currentState: The actual state observation just received.
     */
    void addObservation(const state_sequence& prevState, state_single currentState);
    /**
     * Add multiple observations to the model based on breaking the prevState apart.
     * e.g. prevState: [a, b, c], currentState [d]
     * Adds: [a, b, c]->[d], [b, c]->[d], [c]->[d]
     */
    void addObservationAllOrders(const state_sequence& prevState, state_single currentState);

    /**
     * Breaks a sequence into an array of sequences.
     */
    std::vector<state_sequence> breakStateIntoAllOrders(const state_sequence& prevState);
    /**
     * Attempts to find a matching transition for the supplied observation sequence.
     * @param prevState: A variable length sequence.
     * @param maxOrderWanted: Can force the model to try for a lower max order. Default is the highest possible.
     * @param needChoice: Default is true. True means that when the model picks a lower order to try, it restricts choices to those with >1 observation mapping.
     */
    state_single generateObservation(const state_sequence& prevState, int maxOrderWanted = 10, bool needChoice = true);

    /** Selects a random observation from the model. Used as a fallback when `generateObservation` finds no match. */
    state_single zeroOrderSample();

    /** Removes a specific mapping from the model, if it exists. Used for negative feedback. */
    void removeMapping(state_single state_key, state_single unwanted_option);
    /** Amplifies a specific mapping in the model. Used for positive feedback. */
    void amplifyMapping(state_single state_key, state_single wanted_option);
    /** Gets the sequence mapping for a given state string key. */
    state_sequence getOptionsForSequenceKey(state_single seqAsKey);

    /** Gets the order of the last match made by `generateObservation`. */
    int getOrderOfLastMatch() const;
    /** Gets the state and observation of the last match. */
    state_and_observation getLastMatch() const;

    /** Empties the model mapping. */
    void reset();

    /** Converts the model into a string representation for saving. */
    std::string toString();
    /** Binary representation. */
    std::string toStringBinary() const;

    /** Loads the model from a string. */
    bool fromString(const std::string& savedModel);
    /** Fast loading from a string. */
    bool fromStringFast(const std::string& savedModel);
    /** Loading from a binary string. */
    bool fromStringBinary(const std::string& savedModel);

    /** Tokenises a string based on a character separator. */
    static std::vector<std::string> tokenise(const std::string& input, char separator);

    /** The current number of mappings in the model. */
    size_t getModelSize() const;

    // 🌟 修复: 添加 size() 声明以匹配 .cpp 文件的实现
    long size() const;

private:
    /** Converts a state sequence to a CSV format key: `order,obs1,obs2...` */
    std::string stateSequenceToString(const state_sequence& sequence) const;
    /** Similar to above, but restricts the size to the specified maximum length. */
    std::string stateSequenceToString(const state_sequence& sequence, unsigned long limitOrder) const;

    state_single pickRandomObservation(const state_sequence& seq);
    bool validateStateSequence(const state_sequence& seq) const;
    bool validateStateToObservationsString(const std::string& data) const;

    unsigned long maxOrder; // Maximum order of the chain
    int orderOfLastMatch;   // Last match order integer
    std::map<state_single, state_sequence> model;
    state_and_observation lastMatch;
};