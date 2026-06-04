/*
  ==============================================================================

    MarkovManager.cpp
    Created: 30 Oct 2019 3:28:02pm
    Author:  matthew

  ==============================================================================
*/

#include "MarkovManager.h"
#include <iostream>
#include <fstream>
#include <sstream>

// 🌟 核心修复: 通过初始化列表显式调用 MarkovChain 的构造函数
MarkovManager::MarkovManager(unsigned long maxOrder, unsigned long chainEventMemoryLength)
  : chain(maxOrder), maxChainEventMemory(chainEventMemoryLength), chainEventIndex(0)
{
  inputMemory.assign(maxOrder, "0");
  outputMemory.assign(maxOrder, "0");
}

MarkovManager::~MarkovManager()
{
}

void MarkovManager::reset()
{
  mtx.lock();
  inputMemory.assign(inputMemory.size(), "0");
  outputMemory.assign(outputMemory.size(), "0");
  lastGeneratedOrder = -1;
  sameOrderRepeatCount = 0;
  chain.reset();
  mtx.unlock();
}

void MarkovManager::putEvent(state_single event)
{
  mtx.lock();
  try {
      chain.addObservationAllOrders(inputMemory, event);
      addStateToStateSequence(inputMemory, event);
  } catch(...) {
      std::cout << "MarkovManager::putEvent crashed... catching" << std::endl;
  }
  mtx.unlock();
}

void MarkovManager::observeContextOnly(state_single event)
{
  std::lock_guard<std::mutex> lock(mtx);
  addStateToStateSequence(inputMemory, event);
}

state_single MarkovManager::getEvent(bool needChoices, bool useInputAsContext)
{
  mtx.lock();
  state_single event{""};

  try{
    if (useInputAsContext){
      event = chain.generateObservation(inputMemory, static_cast<int>(outputMemory.size()), needChoices);
    }
    else{
      event = chain.generateObservation(outputMemory, static_cast<int>(outputMemory.size()), needChoices);
    }

    addStateToStateSequence(outputMemory, event);
    rememberChainEvent(chain.getLastMatch());

    const int order = chain.getOrderOfLastMatch();
    if (order == lastGeneratedOrder)
    {
        sameOrderRepeatCount++;
        if (sameOrderRepeatCount >= maxSameOrderRepeats && maxSameOrderRepeats > 0)
        {
            resetGenerationMemory();
        }
    }
    else
    {
        lastGeneratedOrder = order;
        sameOrderRepeatCount = 1;
    }
  } catch(...) {
    std::cout << "MarkovManager::getEvent crashed... catching" << std::endl;
    event = "0";
  }
  mtx.unlock();
  return event;
}

void MarkovManager::addStateToStateSequence(state_sequence& seq, state_single new_state){
  for (size_t i = 1; i < seq.size(); i++)
  {
    seq[i-1] = seq[i];
  }
  seq[seq.size()-1] = new_state;
}

int MarkovManager::getOrderOfLastEvent()
{
  mtx.lock();
  const int order = chain.getOrderOfLastMatch();
  mtx.unlock();
  return order;
}

void MarkovManager::setMaxSameOrderRepeats(unsigned int maxRepeats)
{
  std::lock_guard<std::mutex> lock(mtx);
  maxSameOrderRepeats = maxRepeats;
}

void MarkovManager::resetGenerationMemory()
{
  inputMemory.assign(inputMemory.size(), "0");
  outputMemory.assign(outputMemory.size(), "0");
  lastGeneratedOrder = -1;
  sameOrderRepeatCount = 0;
}

void MarkovManager::rememberChainEvent(state_and_observation sObs)
{
  if (chainEvents.size() < maxChainEventMemory)
  {
    chainEvents.push_back(sObs);
  }
  else
  {
    chainEvents[chainEventIndex] = sObs;
    chainEventIndex = (chainEventIndex + 1) % maxChainEventMemory;
  }
}

void MarkovManager::giveNegativeFeedback()
{
  mtx.lock();
  for (state_and_observation& so : chainEvents)
  {
    chain.removeMapping(so.first, so.second);
  }
  mtx.unlock();
}

void MarkovManager::givePositiveFeedback()
{
  mtx.lock();
  for (state_and_observation& so : chainEvents)
  {
    chain.amplifyMapping(so.first, so.second);
  }
  mtx.unlock();
}

bool MarkovManager::loadModel(const std::string& filename)
{
  if (std::ifstream in {filename})
  {
    std::ostringstream sstr{};
    sstr << in.rdbuf();
    std::string data = sstr.str();
    in.close();
    mtx.lock();
    const bool result = chain.fromStringFast(data);
    mtx.unlock();
    return result;
  }
  else {
    return false;
  }
}

bool MarkovManager::loadModelBinary(const std::string& filename)
{
  if (std::ifstream in{filename, std::ios::binary})
  {
    std::ostringstream sstr{};
    sstr << in.rdbuf();
    std::string data = sstr.str();
    in.close();

    mtx.lock();
    const bool result = chain.fromStringBinary(data);
    mtx.unlock();
    return result;
  }
  else
  {
    return false;
  }
}

bool MarkovManager::saveModel(const std::string& filename)
{
    std::string data;
    mtx.lock();
    data = chain.toString();
    mtx.unlock();

    if (std::ofstream ofs{filename}){
      ofs << data;
      ofs.close();
      return true;
    }
    else {
      std::cout << "MarkovManager::saveModel failed to save to file " << filename << std::endl;
      return false;
    }
}

bool MarkovManager::saveModelBinary(const std::string& filename)
{
    std::string data;
    bool wasEmpty = false;
    mtx.lock();
    data = chain.toStringBinary();
    wasEmpty = (chain.getModelSize() == 0);
    mtx.unlock();

    if (data.empty() && !wasEmpty)
    {
      std::cout << "MarkovManager::saveModelBinary failed to serialise model\n";
      return false;
    }

    if (std::ofstream ofs{filename, std::ios::binary})
    {
      ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
      ofs.close();
      return true;
    }
    else
    {
      std::cout << "MarkovManager::saveModelBinary failed to save to file " << filename << std::endl;
      return false;
    }
}

std::string MarkovManager::getModelAsString()
{
  mtx.lock();
  auto data = chain.toString();
  mtx.unlock();
  return data;
}

std::string MarkovManager::getModelAsBinaryString()
{
  mtx.lock();
  auto data = chain.toStringBinary();
  mtx.unlock();
  return data;
}

bool MarkovManager::setupModelFromString(const std::string& modelData)
{
  mtx.lock();
  const bool result = chain.fromString(modelData);
  mtx.unlock();
  return result;
}

bool MarkovManager::setupModelFromBinaryString(const std::string& modelData)
{
  mtx.lock();
  const bool result = chain.fromStringBinary(modelData);
  mtx.unlock();
  return result;
}

MarkovChain MarkovManager::getCopyOfModel()
{
  mtx.lock();
  auto copy = chain;
  mtx.unlock();
  return copy;
}

size_t MarkovManager::getModelSize()
{
  mtx.lock();
  const auto size = this->chain.getModelSize();
  mtx.unlock();
  return size;
}

int MarkovManager::getLastOrderOfMatch()
{
  std::lock_guard<std::mutex> lock(mtx);
  return chain.getOrderOfLastMatch();
}