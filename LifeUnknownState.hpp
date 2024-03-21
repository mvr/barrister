#pragma once

#include <bit>

#include "LifeAPI.h"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "Transition.hpp"
#include "LifeHistoryState.hpp"

class LifeUnknownState {
public:
  LifeState state;
  LifeState unknown;
  LifeState unknownStable; // Equal to the stable state

  bool operator==(const LifeUnknownState &b) const {
    return state == b.state && unknown == b.unknown && unknownStable == b.unknownStable;
  }
  bool operator!=(const LifeUnknownState &b) const {
    return !(*this == b);
  }

  LifeUnknownState StepMaintaining(const LifeStableState &stable) const;
  std::tuple<uint64_t, uint64_t, uint64_t> StepMaintainingColumn(const LifeStableState &stable, int i) const;
  std::tuple<std::array<uint64_t, 4>, std::array<uint64_t, 4>, std::array<uint64_t, 4>> StepMaintainingStrip(const LifeStableState &stable, int i) const;

  // bool CanCleanlyAdvance(const LifeStableState &stable) const;
  LifeState ActiveComparedTo(const LifeStableState &stable) const;
  LifeState ChangesComparedTo(const LifeUnknownState &prev) const;
  void TransferStable(const LifeStableState &stable);
  void TransferStable(const LifeStableState &stable, std::pair<int, int> cell);

  void SetKnown(std::pair<int, int> cell, bool value, bool stable) {
    if (stable) {
      unknown.Set(cell);
      unknownStable.Set(cell);
    } else {
      state.Set(cell, value);
      unknown.Erase(cell);
      unknownStable.Erase(cell);
    }
  }

  void SetTransitionResult(std::pair<int, int> cell, Transition transition) {
    switch (transition) {
    case Transition::OFF_TO_OFF:
    case Transition::ON_TO_OFF:
      SetKnown(cell, false, false);
      break;
    case Transition::OFF_TO_ON:
    case Transition::ON_TO_ON:
      SetKnown(cell, true, false);
      break;
    case Transition::STABLE_TO_STABLE:
      SetKnown(cell, false, true);
      break;
    default:
      break;
    }
  }

  void SetTransitionPrev(std::pair<int, int> cell, Transition transition) {
    switch (transition) {
    case Transition::OFF_TO_OFF:
    case Transition::OFF_TO_ON:
      SetKnown(cell, false, false);
      break;
    case Transition::ON_TO_OFF:
    case Transition::ON_TO_ON:
      SetKnown(cell, true, false);
      break;
    case Transition::STABLE_TO_STABLE:
      SetKnown(cell, false, true);
      break;
    default:
      break;
    }
  }

  Transition UnperturbedTransitionFor(std::pair<int, int> cell) const {
    bool next = state.StepFor(cell);
    switch (state.Get(cell) << 1 | next) {
    case 0b00: return Transition::OFF_TO_OFF;
    case 0b01: return Transition::OFF_TO_ON;
    case 0b10: return Transition::ON_TO_OFF;
    case 0b11: return Transition::ON_TO_ON;
    default: return Transition::IMPOSSIBLE;
    }
  }

  // Would this transition be an interaction with the active pattern?
  bool TransitionIsPerturbation(std::pair<int, int> cell, Transition transition) const {
    return transition != Transition::STABLE_TO_STABLE && transition != UnperturbedTransitionFor(cell);
  }

  // Would this transition be a active cell?
  bool TransitionIsActive(std::pair<int, int> cell, Transition transition) const {
    return (transition == Transition::ON_TO_OFF ||
            transition == Transition::OFF_TO_ON) &&
           TransitionIsPerturbation(cell, transition);
  }

  std::tuple<bool, bool, bool> StepMaintainingFor(const LifeStableState &stable,
                                                  std::pair<int, int> cell) const {
    auto [next, nextUnknown, nextUnknownStable] = StepMaintainingColumn(stable, cell.first);

    int y = cell.second;
    bool cellNext    = (next                    & (1ULL << y)) >> y;
    bool cellUnknown = (nextUnknown             & (1ULL << y)) >> y;
    bool cellUnknownStable = (nextUnknownStable & (1ULL << y)) >> y;
    return {cellNext, cellUnknown, cellUnknownStable};
  }

  Transition TransitionsFor(const LifeStableState &stable, std::pair<int, int> cell) const {
    bool prevUnknown = unknown.Get(cell) && !(unknownStable.Get(cell) && !stable.unknown.Get(cell));
    bool prevState = state.Get(cell) || (unknownStable.Get(cell) && stable.state.Get(cell));
    auto [nextState, nextUnknown, nextStableUnknown] = StepMaintainingFor(stable, cell);

    Transition transitions = Transition::ANY;
    if (!prevUnknown) {
      if (prevState)
        transitions &= Transition::ON_TO_OFF | Transition::ON_TO_ON;
      else
        transitions &= Transition::OFF_TO_OFF | Transition::OFF_TO_ON;
    }
    if (!nextUnknown) {
      if (nextState)
        transitions &= Transition::OFF_TO_ON | Transition::ON_TO_ON;
      else
        transitions &= Transition::OFF_TO_OFF | Transition::ON_TO_OFF;
    }
    return transitions;
  }
  
  LifeHistoryState ToHistory() const {
    return LifeHistoryState(state, unknown, unknownStable, LifeState());
  }

  friend std::ostream& operator<<(std::ostream& os, LifeUnknownState const& self) {
    return os << self.ToHistory().RLEWHeader();
  }

  void SanityCheck(const LifeStableState &stable) {
#ifdef DEBUG
    assert((unknown & state).IsEmpty());
    assert((~unknown & unknownStable).IsEmpty());
    assert((~unknown & stable.unknown).IsEmpty());
    // assert((unknownStable & ~stable.unknown).IsEmpty());
#endif
  }
};

LifeUnknownState LifeUnknownState::StepMaintaining(const LifeStableState &stable) const {
  LifeUnknownState result{LifeState(false), LifeState(false), LifeState(false)};

  NeighbourCount stateCount(state);
  NeighbourCount unknownCount(unknown);
  NeighbourCount stableCount(stable.state);

  LifeState nearUnstableUnknown = (unknown & ~unknownStable).ZOI();
  LifeState differentCountToStable = stateCount.bit3 |
                                     (stateCount.bit2 ^ stableCount.bit2) |
                                     (stateCount.bit1 ^ stableCount.bit1) |
                                     (stateCount.bit0 ^ stableCount.bit0);

  LifeState restorable = ~((state ^ stable.state) | nearUnstableUnknown | differentCountToStable);
  LifeState refineable = ~(restorable | nearUnstableUnknown);

  #pragma clang loop vectorize_width(4)
  for (int i = 0; i < N; i++) {
    // uint64_t on3 = stateCount.bit3[i];
    uint64_t on2 = stateCount.bit2[i];
    uint64_t on1 = stateCount.bit1[i];
    uint64_t on0 = stateCount.bit0[i];

    uint64_t unk3 = unknownCount.bit3[i];
    uint64_t unk2 = unknownCount.bit2[i];
    uint64_t unk1 = unknownCount.bit1[i];
    uint64_t unk0 = unknownCount.bit0[i];

    uint64_t current_on = state[i];
    uint64_t current_unknown = unknown[i];

    uint64_t naive_next_on = 0;
    uint64_t naive_next_unknown = 0;

    // Begin Autogenerated, see bitslicing/unknown_step.py
#include "bitslicing/unknown_step.hpp"
    // End Autogenerated

    result.state[i] = naive_next_on;
    result.unknown[i] = naive_next_unknown;

    uint64_t toRestore = restorable[i] & result.unknown[i];

    result.state[i] = (result.state[i] & ~toRestore) | (stable.state[i] & toRestore);
    result.unknown[i] = (result.unknown[i] & ~toRestore) | (stable.unknown[i] & toRestore);
    result.unknownStable[i] = stable.unknown[i] & toRestore;
    refineable[i] &= result.unknown[i] & ~result.unknownStable[i];
  }

  if (refineable.IsEmpty())
    return result;

  uint64_t mask = refineable.PopulatedColumns();
  NeighbourCount diff = stableCount.Subtract(mask, stateCount);

  for (auto s : StripIterator(mask)) {
  #pragma clang loop vectorize_width(4)
  for (int i = 0; i < 4; i++) {
    uint64_t current_on = state[s][i];
    uint64_t current_unknown = unknown[s][i];

    uint64_t on3 = stateCount.bit3[i];
    uint64_t on2 = stateCount.bit2[i];
    uint64_t on1 = stateCount.bit1[i];
    uint64_t on0 = stateCount.bit0[i];

    uint64_t unk3 = unknownCount.bit3[s][i];
    uint64_t unk2 = unknownCount.bit2[s][i];
    uint64_t unk1 = unknownCount.bit1[s][i];
    uint64_t unk0 = unknownCount.bit0[s][i];

    uint64_t l2 = stable.live2[s][i];
    uint64_t l3 = stable.live3[s][i];
    uint64_t d0 = stable.dead0[s][i];
    uint64_t d1 = stable.dead1[s][i];
    uint64_t d2 = stable.dead2[s][i];
    uint64_t d4 = stable.dead4[s][i];
    uint64_t d5 = stable.dead5[s][i];
    uint64_t d6 = stable.dead6[s][i];

    uint64_t m3 = diff.bit3[s][i];
    uint64_t m2 = diff.bit2[s][i];
    uint64_t m1 = diff.bit1[s][i];
    uint64_t m0 = diff.bit0[s][i];

    uint64_t next_on = 0;
    uint64_t next_unknown = 0;
    uint64_t next_unknown_stable = 0;

    // Begin Autogenerated, see bitslicing/unknown_step_refined.py
#include "bitslicing/unknown_step_refined_m.hpp"
    // End Autogenerated

    // Handle the DONTCAREs
    next_unknown |= current_unknown;
    next_on &= ~next_unknown;
    next_unknown_stable &= current_unknown;

    result.state[s][i] = (result.state[s][i] & ~refineable[s][i]) | (next_on & refineable[s][i]);
    result.unknown[s][i] = (result.unknown[s][i] & ~refineable[s][i]) | (next_unknown & refineable[s][i]);
    result.unknownStable[s][i] = (result.unknownStable[s][i] & ~refineable[s][i]) | (next_unknown_stable & refineable[s][i]);
  }
  }
  return result;
}

std::tuple<uint64_t, uint64_t, uint64_t> LifeUnknownState::StepMaintainingColumn(const LifeStableState &stable, int i) const {
  auto [on3, on2, on1, on0] = CountNeighbourhoodColumn(state, i);
  auto [unk3, unk2, unk1, unk0] = CountNeighbourhoodColumn(unknown, i);
  auto [s3, s2, s1, s0] = CountNeighbourhoodColumn(stable.state, i);

  uint64_t stateon = state[i];
  uint64_t stateunk = unknown[i];
  uint64_t stateunkstable = unknownStable[i];

  uint64_t current_on = state[i];
  uint64_t current_unknown = unknown[i];
  uint64_t stable_on = stable.state[i];
  uint64_t stable_unknown = stable.unknown[i];

  uint64_t naive_next_on = 0;
  uint64_t naive_next_unknown = 0;

  // Begin Autogenerated, see bitslicing/unknown_step.py
#include "bitslicing/unknown_step.hpp"
  // End Autogenerated

  uint64_t l2 = stable.live2[i];
  uint64_t l3 = stable.live3[i];
  uint64_t d0 = stable.dead0[i];
  uint64_t d1 = stable.dead1[i];
  uint64_t d2 = stable.dead2[i];
  uint64_t d4 = stable.dead4[i];
  uint64_t d5 = stable.dead5[i];
  uint64_t d6 = stable.dead6[i];

  uint64_t next_on = 0;
  uint64_t next_unknown = 0;
  uint64_t next_unknown_stable = 0;

  // Begin Autogenerated, see bitslicing/unknown_step_refined.py
#include "bitslicing/unknown_step_refined.hpp"
  // End Autogenerated

  // Handle the DONTCAREs
  next_unknown |= current_unknown;
  next_on &= ~next_unknown;
  next_unknown_stable &= current_unknown;

  uint64_t activeUnknownZOI = (unknown & ~unknownStable).ZOIColumn(i);
  uint64_t useRefined = ~activeUnknownZOI;

  uint64_t result_state = (naive_next_on & ~useRefined) | (next_on & useRefined);
  uint64_t result_unknown = (naive_next_unknown & ~useRefined) | (next_unknown & useRefined);
  uint64_t result_unknownStable = (next_unknown_stable & useRefined);

  return {result_state, result_unknown, result_unknownStable};
}

std::tuple<std::array<uint64_t, 4>,
           std::array<uint64_t, 4>,
           std::array<uint64_t, 4>>
LifeUnknownState::StepMaintainingStrip(const LifeStableState &stable, int column) const {
  std::array<uint64_t, 4> resultState;
  std::array<uint64_t, 4> resultUnknown;
  std::array<uint64_t, 4> resultUnknownStable;

  std::array<uint64_t, 6> nearbyState = state.GetStrip<6>(column);
  std::array<uint64_t, 6> nearbyUnknown = unknown.GetStrip<6>(column);
  std::array<uint64_t, 6> nearbyStableState = stable.state.GetStrip<6>(column);
  std::array<uint64_t, 6> nearbyStableUnknown = stable.unknown.GetStrip<6>(column);

  std::array<uint64_t, 4> nearbylive2 = stable.live2.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbylive3 = stable.live3.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead0 = stable.dead0.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead1 = stable.dead1.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead2 = stable.dead2.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead4 = stable.dead4.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead5 = stable.dead5.GetStrip<4>(column);
  std::array<uint64_t, 4> nearbydead6 = stable.dead6.GetStrip<4>(column);

  std::array<uint64_t, 4> state3, state2, state1, state0;
  std::array<uint64_t, 4> unknown3, unknown2, unknown1, unknown0;
  CountNeighbourhoodStrip(nearbyState, state3, state2, state1, state0);
  CountNeighbourhoodStrip(nearbyUnknown, unknown3, unknown2, unknown1, unknown0);

  std::array<uint64_t, 4> stable3, stable2, stable1, stable0;
  CountNeighbourhoodStrip(nearbyStableState, stable3, stable2, stable1, stable0);

  std::array<uint64_t, 4> diff3, diff2, diff1, diff0;
  FourBitSubtract(stable3, stable2, stable1, stable0,
                  state3, state2, state1, state0,
                  diff3, diff2, diff1, diff0);

  // TODO wasteful
  LifeState unknownActive = unknown & ~unknownStable;
  std::array<uint64_t, 4> useRefined;
  for (int i = 1; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    useRefined[i-1] = ~unknownActive.ZOIColumn(c);
  }

  #pragma clang loop vectorize_width(4)
  for (int i = 1; i < 5; i++) {
    uint64_t on3 = state3[i-1];
    uint64_t on2 = state2[i-1];
    uint64_t on1 = state1[i-1];
    uint64_t on0 = state0[i-1];

    uint64_t unk3 = unknown3[i-1];
    uint64_t unk2 = unknown2[i-1];
    uint64_t unk1 = unknown1[i-1];
    uint64_t unk0 = unknown0[i-1];

    uint64_t current_on = nearbyState[i];
    uint64_t current_unknown = nearbyUnknown[i];
    uint64_t stable_on = nearbyStableState[i];
    uint64_t stable_unknown = nearbyStableUnknown[i];

    uint64_t naive_next_on = 0;
    uint64_t naive_next_unknown = 0;

    // Begin Autogenerated, see bitslicing/unknown_step.py
#include "bitslicing/unknown_step.hpp"
    // End Autogenerated

    uint64_t l2 = nearbylive2[i-1];
    uint64_t l3 = nearbylive3[i-1];
    uint64_t d0 = nearbydead0[i-1];
    uint64_t d1 = nearbydead1[i-1];
    uint64_t d2 = nearbydead2[i-1];
    uint64_t d4 = nearbydead4[i-1];
    uint64_t d5 = nearbydead5[i-1];
    uint64_t d6 = nearbydead6[i-1];

    uint64_t m3 = diff3[i-1];
    uint64_t m2 = diff2[i-1];
    uint64_t m1 = diff1[i-1];
    uint64_t m0 = diff0[i-1];

    uint64_t next_on = 0;
    uint64_t next_unknown = 0;
    uint64_t next_unknown_stable = 0;

    // Begin Autogenerated, see bitslicing/unknown_step_refined.py
#include "bitslicing/unknown_step_refined_m.hpp"
    // End Autogenerated

    // Handle the DONTCAREs
    next_unknown |= current_unknown;
    next_on &= ~next_unknown;
    next_unknown_stable &= current_unknown;

    resultState[i-1] = (naive_next_on & ~useRefined[i-1]) | (next_on & useRefined[i-1]);
    resultUnknown[i-1] = (naive_next_unknown & ~useRefined[i-1]) | (next_unknown & useRefined[i-1]);
    resultUnknownStable[i-1] = (next_unknown_stable & useRefined[i-1]);
  }

  return {resultState, resultUnknown, resultUnknownStable};
}

LifeState LifeUnknownState::ActiveComparedTo(const LifeStableState &stable) const {
  return ~unknown & ~stable.unknown & (stable.state ^ state);
}

void LifeUnknownState::TransferStable(const LifeStableState &stable) {
  LifeState updated = unknownStable & ~stable.unknown;
  state |= stable.state & updated;
  unknown &= ~updated;
  unknownStable &= ~updated;
}

void LifeUnknownState::TransferStable(const LifeStableState &stable, std::pair<int, int> cell) {
  bool updated = unknownStable.Get(cell) && !stable.unknown.Get(cell);
  if (updated) {
    if(stable.state.Get(cell))
      state.Set(cell);
    unknown.Erase(cell);
    unknownStable.Erase(cell);
  }
}

LifeState
LifeUnknownState::ChangesComparedTo(const LifeUnknownState &prev) const {
  return (state ^ prev.state) & ~unknown & ~prev.unknown;
}
