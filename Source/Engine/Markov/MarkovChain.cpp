/*
  ==============================================================================

    MarkovChain.cpp
    Created: 25 Oct 2019 6:47:13am
    Author:  matthew

  ==============================================================================
*/

#include "MarkovChain.h"
#include <iostream>
#include <ctime>
#include <limits>
#include <algorithm>
#include <functional>

namespace
{
inline void appendUint32(std::string& dest, uint32_t value)
{
  dest.push_back(static_cast<char>(value & 0xFFu));
  dest.push_back(static_cast<char>((value >> 8) & 0xFFu));
  dest.push_back(static_cast<char>((value >> 16) & 0xFFu));
  dest.push_back(static_cast<char>((value >> 24) & 0xFFu));
}

inline bool readUint32(const std::string& src, size_t& offset, uint32_t& value)
{
  if (offset + 4 > src.size())
    return false;

  const auto b0 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset]));
  const auto b1 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 1]));
  const auto b2 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 2]));
  const auto b3 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 3]));
  value = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
  offset += 4;
  return true;
}
}

MarkovChain::MarkovChain(unsigned long  _maxOrder) : maxOrder{_maxOrder}, orderOfLastMatch{0}
{
  srand(static_cast<unsigned int>(time(nullptr)));
}

MarkovChain::~MarkovChain()
{
}

void MarkovChain::addObservation(const state_sequence& prevState, state_single currentState)
{
  if (currentState == "0") return;

  if (!validateStateSequence(prevState)) return;

  state_single key = stateSequenceToString(prevState);
  bool have_key = true;
  try {
    model.at(key);
  } catch (const std::out_of_range&) {
    have_key = false;
  }

  if (have_key) {
    model[key].push_back(currentState);
  } else {
    state_sequence seq = {currentState};
    model[key] = seq;
  }
}

void MarkovChain::addObservationAllOrders(const state_sequence& prevState, state_single currentState)
{
  std::vector<state_sequence> allPrevs = breakStateIntoAllOrders(prevState);
  for (state_sequence& seq : allPrevs)
  {
    addObservation(seq, currentState);
  }
}

std::vector<state_sequence> MarkovChain::breakStateIntoAllOrders(const state_sequence& prevState)
{
  std::vector<state_sequence> allPrevs;
  size_t end = prevState.size();
  allPrevs.push_back(prevState);
  for (size_t start = 1; start < end; ++start)
  {
    state_sequence::const_iterator first = prevState.begin() + static_cast<ptrdiff_t>(start);
    state_sequence::const_iterator last = prevState.begin() + static_cast<ptrdiff_t>(prevState.size());
    state_sequence prevStateShort(first, last);
    allPrevs.push_back(prevStateShort);
  }
  return allPrevs;
}

std::string MarkovChain::stateSequenceToString(const state_sequence& sequence) const
{
  std::string str = std::to_string(sequence.size());
  str.append(",");
  for (const state_single& s : sequence)
  {
      str.append(s);
      str.append(",");
  }
  return str;
}

std::string MarkovChain::stateSequenceToString(const state_sequence& sequence, unsigned long limitOrder) const
{
  if (limitOrder >= sequence.size()){
    return stateSequenceToString(sequence);
  }
  else {
    std::string str = std::to_string(limitOrder);
    str.append(",");
    unsigned long want_to_skip = static_cast<unsigned long>(sequence.size()) - limitOrder;
    unsigned long skipped = 0;

    for (const state_single& s : sequence)
    {
     if (skipped < want_to_skip)
     {
        skipped++;
        continue;
     }
      str.append(s);
      str.append(",");
    }
    return str;
  }
}

state_single MarkovChain::generateObservation(const state_sequence& prevState, int maxOrderWanted, bool needChoice)
{
  if (model.empty())
    return "0";

  if (maxOrderWanted > static_cast<int>(this->maxOrder))
      maxOrderWanted = static_cast<int>(this->maxOrder);

  auto countUsableOrder = [&](const state_sequence& seq, int order) -> int
  {
      const int usableLen = std::max(0, order);
      const int startIndex = static_cast<int>(seq.size()) - usableLen;
      int effective = 0;
      for (int i = std::max(0, startIndex); i < static_cast<int>(seq.size()); ++i)
      {
          if (seq[static_cast<size_t>(i)] != "0")
              ++effective;
      }
      return std::clamp(effective, 0, usableLen);
  };

  std::function<state_single(int, int&)> recurse = [&](int orderLimit, int& matchedOrder) -> state_single
  {
      const int effectiveOrder = countUsableOrder(prevState, orderLimit);
      state_single key = stateSequenceToString(prevState, static_cast<unsigned long>(orderLimit));
      state_sequence poss_next_states{};
      bool have_key = true;
      try
      {
          poss_next_states = model.at(key);
          if (needChoice && poss_next_states.size() < 2)
              have_key = false;
      }
      catch (const std::out_of_range&)
      {
          have_key = false;
      }

      if (have_key)
      {
          state_single obs = pickRandomObservation(poss_next_states);
          matchedOrder = effectiveOrder;
          lastMatch = state_and_observation{ key, obs };
          return obs;
      }

      if (orderLimit > 1)
          return recurse(orderLimit - 1, matchedOrder);

      matchedOrder = 0;
      state_single obs = zeroOrderSample();
      lastMatch = state_and_observation{ "0", obs };
      return obs;
  };

  int matchedOrder = 0;
  state_single result = recurse(maxOrderWanted, matchedOrder);
  orderOfLastMatch = matchedOrder;
  return result;
}

state_single MarkovChain::zeroOrderSample()
{
  state_sequence poss_next_states{};
  size_t randInd = 0;
  if (model.size() > 1) randInd = static_cast<size_t>(rand()) % model.size();

  size_t ind = 0;
  state_single state = "0";
  for (auto it = model.begin(); it != model.end(); ++it)
  {
    if (ind == randInd){
      poss_next_states = (it->second);
      state = pickRandomObservation(poss_next_states);
      break;
    }
    else {
      ind++;
      continue;
    }
  }
  return state;
}

state_single MarkovChain::pickRandomObservation(const state_sequence& seq)
{
  if (seq.empty()) return "0";
  size_t ind = 0;
  if (seq.size() > 1) ind = static_cast<size_t>(rand()) % seq.size();
  return seq.at(ind);
}

std::string MarkovChain::toString()
{
  std::string s{""};
  for(auto const& kv_pair: model){
    s += kv_pair.first + ":";
    s += this->stateSequenceToString(model[kv_pair.first]);
    s += "\n";
  }
  return s;
}

std::string MarkovChain::toStringBinary() const
{
  std::string buffer;
  buffer.reserve(model.size() * 32);

  appendUint32(buffer, static_cast<uint32_t>(model.size()));

  for (const auto& kv : model)
  {
    const auto& key = kv.first;
    const auto& values = kv.second;

    if (key.size() > std::numeric_limits<uint32_t>::max())
      return {};

    appendUint32(buffer, static_cast<uint32_t>(key.size()));
    buffer.append(key.data(), key.size());

    if (values.size() > std::numeric_limits<uint32_t>::max())
      return {};

    appendUint32(buffer, static_cast<uint32_t>(values.size()));
    for (const auto& obs : values)
    {
      if (obs.size() > std::numeric_limits<uint32_t>::max())
        return {};

      appendUint32(buffer, static_cast<uint32_t>(obs.size()));
      buffer.append(obs.data(), obs.size());
    }
  }

  return buffer;
}

bool MarkovChain::validateStateToObservationsString(const std::string& data) const
{
  if (data.size() < 7) {
    return false;
  }
  if (data.find_first_of(':') == std::string::npos) {
    return false;
  }
  auto found = data.find_first_of(',');
  int count = 0;
  while (found != std::string::npos)
  {
    count++;
    if (count > 1) break;
    found = data.find_first_of(',', found + 1);
  }
  if (count < 2){
    return false;
  }
  return true;
}

bool MarkovChain::fromString(const std::string& savedModel)
{
  std::vector<std::string> lines = MarkovChain::tokenise(savedModel, '\n');
  for (const std::string& line : lines){
    if (!MarkovChain::validateStateToObservationsString(line)) continue;

    std::vector<std::string> k_v = MarkovChain::tokenise(line, ':');
    state_sequence prevState = MarkovChain::tokenise(k_v[0], ',');
    state_sequence prevStateFilt{};
    if (prevState.size() <= 1) continue;

    for (size_t i = 1; i < prevState.size(); ++i){
      prevStateFilt.push_back(prevState[i]);
    }
    state_sequence all_obs = MarkovChain::tokenise(k_v[1], ',');
    if (all_obs.size() <= 1) continue;
    for (size_t i = 1; i < all_obs.size(); ++i){
      this->addObservation(prevStateFilt, all_obs[i]);
    }
  }
  return true;
}

bool MarkovChain::fromStringFast(const std::string& savedModel)
{
  const size_t total = savedModel.size();
  if (total == 0) return true;

  state_sequence prevState;
  prevState.reserve(maxOrder);

  size_t lineStart = 0;
  while (lineStart < total)
  {
    const size_t newlinePos = savedModel.find('\n', lineStart);
    const bool hasNewline = (newlinePos != std::string::npos) && (newlinePos < total);
    const size_t lineEnd = hasNewline ? newlinePos : total;

    if (lineEnd <= lineStart)
    {
      lineStart = hasNewline ? lineEnd + 1 : total;
      continue;
    }

    const size_t colonPos = savedModel.find(':', lineStart);
    if (colonPos == std::string::npos || colonPos >= lineEnd)
    {
      lineStart = hasNewline ? lineEnd + 1 : total;
      continue;
    }

    prevState.clear();
    size_t tokenStart = lineStart;
    int keyTokenCount = 0;

    while (tokenStart < colonPos)
    {
      if (savedModel[tokenStart] == ',')
      {
        ++tokenStart;
        continue;
      }

      size_t tokenEnd = savedModel.find(',', tokenStart);
      if (tokenEnd == std::string::npos || tokenEnd > colonPos)
        tokenEnd = colonPos;

      if (tokenEnd > tokenStart)
      {
        if (keyTokenCount > 0)
          prevState.emplace_back(savedModel.data() + tokenStart, tokenEnd - tokenStart);

        ++keyTokenCount;
      }
      tokenStart = tokenEnd + 1;
    }

    if (keyTokenCount <= 1 || prevState.empty())
    {
      lineStart = hasNewline ? lineEnd + 1 : total;
      continue;
    }

    size_t obsStart = colonPos + 1;
    int obsTokenCount = 0;
    state_single obs;

    while (obsStart < lineEnd)
    {
      if (savedModel[obsStart] == ',')
      {
        ++obsStart;
        continue;
      }

      size_t obsEnd = savedModel.find(',', obsStart);
      if (obsEnd == std::string::npos || obsEnd > lineEnd)
        obsEnd = lineEnd;

      if (obsEnd > obsStart)
      {
        if (obsTokenCount > 0)
        {
          obs.assign(savedModel.data() + obsStart, obsEnd - obsStart);
          addObservation(prevState, obs);
        }
        ++obsTokenCount;
      }
      obsStart = obsEnd + 1;
    }
    lineStart = hasNewline ? lineEnd + 1 : total;
  }
  return true;
}

bool MarkovChain::fromStringBinary(const std::string& savedModel)
{
  size_t offset = 0;
  uint32_t entryCount = 0;

  if (!readUint32(savedModel, offset, entryCount))
    return false;

  std::map<state_single, state_sequence> parsed;

  for (uint32_t i = 0; i < entryCount; ++i)
  {
    uint32_t keySize = 0;
    if (!readUint32(savedModel, offset, keySize))
      return false;

    if (offset + keySize > savedModel.size())
      return false;

    state_single key(savedModel.data() + offset, keySize);
    offset += keySize;

    uint32_t valueCount = 0;
    if (!readUint32(savedModel, offset, valueCount))
      return false;

    state_sequence values;
    values.reserve(valueCount);

    for (uint32_t v = 0; v < valueCount; ++v)
    {
      uint32_t obsSize = 0;
      if (!readUint32(savedModel, offset, obsSize))
        return false;

      if (offset + obsSize > savedModel.size())
        return false;

      values.emplace_back(savedModel.data() + offset, obsSize);
      offset += obsSize;
    }
    parsed.emplace(std::move(key), std::move(values));
  }
  model.swap(parsed);
  return true;
}

void MarkovChain::reset()
{
    model.clear();
}

int MarkovChain::getOrderOfLastMatch() const
{
  return this->orderOfLastMatch;
}

state_and_observation MarkovChain::getLastMatch() const
{
  return this->lastMatch;
}

void MarkovChain::removeMapping(state_single state_key, state_single unwanted_option)
{
  if (model.empty()) return;
  state_sequence current_options{};
  bool have_key = true;
  try
  {
    current_options = model.at(state_key);
  }
  catch (const std::out_of_range&)
  {
    have_key = false;
  }
  if (have_key)
  {
    state_sequence updated_options{};
    for (const state_single& obs : current_options)
    {
      if (obs != unwanted_option)
      {
        updated_options.push_back(obs);
      }
    }
    this->model[state_key] = updated_options;
  }
}

void MarkovChain::amplifyMapping(state_single state_key, state_single wanted_option)
{
  if (model.empty()) return;
  state_sequence options = getOptionsForSequenceKey(state_key);
  if (options.empty())
  {
    options.push_back(wanted_option);
    this->model[state_key] = options;
    return;
  }

  float wantedCount = 0;
  float othermappings = 0;
  for (const state_single& s : options) {
    if (s == wanted_option) wantedCount++;
    else othermappings++;
  }

  for (int i = 0; i < static_cast<int>(othermappings); i++) {
    model[state_key].push_back(wanted_option);
  }
}

state_sequence MarkovChain::getOptionsForSequenceKey(state_single seqAsKey)
{
  state_sequence options{};
  try
  {
    options = model.at(seqAsKey);
  }
  catch (const std::out_of_range&)
  {
  }
  return options;
}

std::vector<std::string> MarkovChain::tokenise(const std::string& input, char separator)
{
   std::vector<std::string> tokens;
   size_t start, end;
   std::string token;
   start = input.find_first_not_of(separator, 0);
   do {
     end = input.find_first_of(separator, start);
     if (start == input.length() || start == end) break;
     if (end != std::string::npos) token = input.substr(start, end - start);
     else token = input.substr(start, input.length() - start);
     tokens.push_back(token);
     start = end + 1;
   } while(end != std::string::npos);

   return tokens;
}

long MarkovChain::size() const
{
  return static_cast<long>(model.size());
}

bool MarkovChain::validateStateSequence(const state_sequence& seq) const
{
  if (seq.empty()) return false;
  for (const state_single& s : seq)
  {
    if (s == "0") return false;
  }
  return true;
}

size_t MarkovChain::getModelSize() const
{
  return this->model.size();
}