#include <fstream>

#include "LifeAPI.h"

#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"

enum RotorType {
  NORESULT,
  OSC,     // repeats
  FIZZLER, // returns to starting state
  STABLE,  // stabilizes to something else
  OTHER    // hit end gen.
};

struct Rotor {
  RotorType type;
  unsigned period;
  unsigned pop;
  std::pair<unsigned, unsigned> size;
  std::string desc;
  std::string comment;

  std::string ToString() const {
    std::string firstLetter;
    switch (type) {
    case RotorType::OSC:
      firstLetter += "p";
      break;
    case RotorType::FIZZLER:
      firstLetter += "f";
      break;
    }

    return firstLetter + std::to_string(period) +
           " r" + std::to_string(pop) + " " +
           std::to_string(size.second) + "x" + std::to_string(size.first) + " " +
           desc;
  }

  int compare(const Rotor &other) const {
    return ToString().compare(other.ToString());
  }
};

// assumes rotor occupies 0 <= x < dims.first, 0 <= y < dims.second
Rotor GetUnnormalisedRotorDesc(const LifeState &genZero,
                                     const LifeState &stator,
                                     const LifeState &rotor,
                                     std::pair<int, int> dims, unsigned period,
                                     RotorType resType) {

  // rotor descriptions do (height)x(width); input is (width, height)
  std::string desc;

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
  return {RotorType::OSC, period, rotor.GetPop(), dims, desc, ""};
}

// assumes rotor occupies 0 <= x < dims.first, 0 <= y < dims.second
// only handles a single gen.
Rotor GetPhaseRotorDesc(const LifeState &genZero, const LifeState &stator,
                              const LifeState &rotor, std::pair<int, int> dims,
                              unsigned period, RotorType resType) {
  Rotor minimalRotorDesc = {};
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

    Rotor rotorDesc =
        GetUnnormalisedRotorDesc(transfGenZero, transfStator, transfRotor,
                                 transfRotor.WidthHeight(), period, resType);
    if (minimalRotorDesc.desc.size() == 0 || minimalRotorDesc.compare(rotorDesc) > 0)
      minimalRotorDesc = rotorDesc;
  }
  return minimalRotorDesc;
}

Rotor GetRotorDesc(const std::vector<LifeState> &states, RotorType res) {
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

  Rotor minDescSoFar;
  for (auto genZeroState : states) {
    genZeroState.Move(-bounds[0], -bounds[1]);
    Rotor desc =
        GetPhaseRotorDesc(genZeroState, stator, rotor,
                          rotor.WidthHeight(), period, res);
    if (minDescSoFar.desc.size() == 0 || minDescSoFar.compare(desc) > 0)
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

Rotor GetRotorDesc(const LifeUnknownState &state, const LifeStableState &stable, unsigned period) {
  LifeUnknownState current = state;
  std::vector<LifeState> states;
  for (unsigned i = 0; i < period; i++) {
    states.push_back(current.state);
    current = current.StepMaintaining(stable);
  }
  return GetRotorDesc(states, RotorType::OSC);
}

std::vector<Rotor> GetSeparatedRotorDesc(const LifeUnknownState &state, const LifeStableState &stable, unsigned period) {
  LifeUnknownState current = state;
  std::vector<LifeState> states;

  LifeState stator = ~LifeState();
  LifeState rotor  = LifeState();
  for (unsigned i = 0; i < period; i++) {
    stator &= current.state;
    rotor |= current.state;
    states.push_back(current.state);
    current = current.StepMaintaining(stable);
  }
  rotor &= ~stator;

  std::vector<Rotor> result;
  for (auto &c : rotor.Components()) {
    LifeState rotorMask = c.BigZOI();
    std::vector<LifeState> rotorStates;
    for (unsigned i = 0; i < period; i++) {
      LifeState thisPhase = states[i] & rotorMask;
      if(i > 0 && thisPhase == rotorStates[0])
        break;
      rotorStates.push_back(thisPhase);
    }
    result.push_back(GetRotorDesc(rotorStates, RotorType::OSC));
  }
  return result;
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
