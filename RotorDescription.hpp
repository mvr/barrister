#include <fstream>

#include "LifeAPI.h"

#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"

enum ResultType {
  NORESULT,
  OSC,     // repeats
  FIZZLER, // returns to starting state
  STABLE,  // stabilizes to something else
  OTHER    // hit end gen.
};

struct Rotor {
  unsigned period;
  std::string desc;
  std::string comment;
  ResultType type;
};

// assumes rotor occupies 0 <= x < dims.first, 0 <= y < dims.second
std::string GetUnnormalisedRotorDesc(const LifeState &genZero,
                                     const LifeState &stator,
                                     const LifeState &rotor,
                                     std::pair<int, int> dims, int period,
                                     ResultType resType) {
  std::string firstLetter;
  switch (resType) {
  case ResultType::OSC:
    firstLetter = "p";
    break;
  case ResultType::FIZZLER:
    firstLetter = "f";
    break;
  }

  // rotor descriptions do (height)x(width); input is (width, height)
  std::string desc = firstLetter + std::to_string(period) + " r" +
                     std::to_string(rotor.GetPop()) + " " +
                     std::to_string(dims.second) + "x" +
                     std::to_string(dims.first) + " ";

  for (int row = 0; row < dims.second; ++row) {
    for (int col = 0; col < dims.first; ++col) {
      if (rotor.Get(col, row))
        desc += std::string(
            1, static_cast<char>(48 + 16 * genZero.Get(col, row) +
                                 stator.CountNeighbours({col, row})));
      else
        desc += ".";
    }
    if (row + 1 != dims.second)
      desc += " ";
  }
  return desc;
}

// assumes rotor occupies 0 <= x < dims.first, 0 <= y < dims.second
// only handles a single gen.
std::string GetPhaseRotorDesc(const LifeState &genZero, const LifeState &stator,
                              const LifeState &rotor, std::pair<int, int> dims,
                              int period, ResultType resType) {
  std::string minimalRotorDesc = "";
  // if non-square half of these transformations are redundant (we always choose
  // an orientation where width >= height) but whatever.

  for (auto transf :
       {Identity, Rotate90, Rotate180OddBoth, Rotate270, ReflectAcrossYeqX,
        ReflectAcrossX, ReflectAcrossYeqNegXP1, ReflectAcrossY}) {

    // shift so upper left corner is at origin.
    std::pair<int, int> shiftBy;
    switch (transf) {
    case (Identity):
    case (ReflectAcrossYeqX):
      shiftBy = std::make_pair(0, 0);
      break;
    case (Rotate90):
      shiftBy = std::make_pair(dims.second - 1, 0);
      break;
    case (ReflectAcrossY):
      shiftBy = std::make_pair(dims.first - 1, 0);
      break;
    case (Rotate180OddBoth):
      shiftBy = std::make_pair(dims.first - 1, dims.second - 1);
      break;
    case (ReflectAcrossX):
      shiftBy = std::make_pair(0, dims.second - 1);
      break;
    case (Rotate270):
      shiftBy = std::make_pair(0, dims.first - 1);
      break;
    case (ReflectAcrossYeqNegXP1):
      shiftBy = std::make_pair(dims.second - 1, dims.first - 1);
      break;
    }

    // could make a helper function here.
    LifeState transfGenZero = genZero;
    transfGenZero.Transform(transf);
    transfGenZero.Move(shiftBy.first, shiftBy.second);
    LifeState transfStator = stator;
    transfStator.Transform(transf);
    transfStator.Move(shiftBy.first, shiftBy.second);
    LifeState transfRotor = rotor;
    transfRotor.Transform(transf);
    transfRotor.Move(shiftBy.first, shiftBy.second);

    std::string rotorDesc =
        GetUnnormalisedRotorDesc(transfGenZero, transfStator, transfRotor,
                                 transfRotor.WidthHeight(), period, resType);
    if (minimalRotorDesc.size() == 0 || minimalRotorDesc.compare(rotorDesc) > 0)
      minimalRotorDesc = rotorDesc;
  }
  return minimalRotorDesc;
}

std::string GetRotorDesc(const std::vector<LifeState> &states, ResultType res) {
  unsigned period = states.size();

  LifeState stator = ~LifeState();
  LifeState rotor  = LifeState();
  for (auto &state : states) {
    stator &= state;
    rotor |= state;
  }
  rotor &= ~stator;

  auto bounds = rotor.XYBounds();
  rotor.Move(-bounds[0], -bounds[1]);
  stator.Move(-bounds[0], -bounds[1]);

  std::string minDescSoFar;
  for (auto genZeroState : states) {
    genZeroState.Move(-bounds[0], -bounds[1]);
    std::string desc =
        GetPhaseRotorDesc(genZeroState, stator, rotor,
                          rotor.WidthHeight(), period, res);
    if (minDescSoFar.size() == 0 || minDescSoFar.compare(desc) > 0)
      minDescSoFar = desc;
  }
  return minDescSoFar;
}


unsigned DeterminePeriod(const LifeUnknownState &state, const LifeStableState &stable) {
  LifeUnknownState current = state;

  std::stack<std::pair<uint64_t, int>> minhashes;

  // TODO: is 60 reasonable?
  for (unsigned i = 1; i < 60; i++) {
    LifeState active = stable.state ^ current.state;

    uint64_t newhash = active.GetHash();

    while(true) {
      if(minhashes.empty())
        break;
      if(minhashes.top().first < newhash)
        break;

      if(minhashes.top().first == newhash) {
        unsigned p = i - minhashes.top().second;
        return p;
      }

      if(minhashes.top().first > newhash)
        minhashes.pop();
    }

    minhashes.push({newhash, i});

    current = current.StepMaintaining(stable);
  }
  return 0;
}

std::string GetRotorDesc(const LifeUnknownState &state, const LifeStableState &stable, unsigned period) {
  LifeUnknownState current = state;
  std::vector<LifeState> states;
  for (unsigned i = 0; i < period; i++) {
    states.push_back(current.state);
    current = current.StepMaintaining(stable);
  }
  return GetRotorDesc(states, ResultType::OSC);
}

std::vector<std::string> ReadRotors(std::string filename) {
  std::vector<std::string> result;
  std::ifstream infile(filename);
  std::string line;
  while (std::getline(infile, line)) {
    auto tabItr = std::find(line.begin(), line.end(), '\t');
    result.emplace_back(line.begin(), tabItr);
  }
  return result;
}
