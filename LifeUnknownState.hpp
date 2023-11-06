#pragma once

#include <bit>

#include "LifeAPI.h"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeHistoryState.hpp"

// Maybe this should just be a bitset + constants
enum class Transition : unsigned char {
  OFF_TO_OFF       = 1 << 0,
  OFF_TO_ON        = 1 << 1,
  ON_TO_OFF        = 1 << 2,
  ON_TO_ON         = 1 << 3,
  STABLE_TO_STABLE = 1 << 4,

  IMPOSSIBLE       = 0,
  ANY = OFF_TO_OFF | OFF_TO_ON | ON_TO_OFF | ON_TO_ON | STABLE_TO_STABLE,
  UNCHANGING = OFF_TO_OFF | ON_TO_ON | STABLE_TO_STABLE,
};

constexpr inline Transition operator~ (Transition a) { return static_cast<Transition>( ~static_cast<std::underlying_type<Transition>::type>(a) ); }
constexpr inline Transition operator| (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) | static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition operator& (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) & static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition operator^ (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) ^ static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition& operator|= (Transition& a, Transition b) { a = a | b; return a; }
constexpr inline Transition& operator&= (Transition& a, Transition b) { a = a & b; return a; }
constexpr inline Transition& operator^= (Transition& a, Transition b) { a = a ^ b; return a; }

Transition TransitionLowest(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return static_cast<Transition>(bits & (~bits + 1));
}

Transition TransitionHighest(Transition t) {
  if (t == Transition::IMPOSSIBLE) return Transition::IMPOSSIBLE;
  unsigned int bits = static_cast<unsigned int>(t);
  return static_cast<Transition>(static_cast<unsigned char>(1 << (8*sizeof(bits)-1-__builtin_clz(bits))));
}

bool TransitionIsSingleton(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return bits && !(bits & (bits-1));
}

unsigned TransitionCount(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return __builtin_popcount(bits);
}

Transition TransitionSimplify(Transition transition) {
  bool has_on  = (transition & Transition::ON_TO_ON)   == Transition::ON_TO_ON;
  bool has_off = (transition & Transition::OFF_TO_OFF) == Transition::OFF_TO_OFF;
  bool has_stable = (transition & Transition::STABLE_TO_STABLE) == Transition::STABLE_TO_STABLE;

  // No need to branch them separately
  if (has_on && has_off) {
    transition |= Transition::STABLE_TO_STABLE;
    transition &= ~(Transition::ON_TO_ON | Transition::OFF_TO_OFF);
  }
  if (has_on && !has_off) transition &= ~Transition::STABLE_TO_STABLE;
  if (!has_on && has_off) transition &= ~Transition::STABLE_TO_STABLE;
  if (has_stable) transition &= ~(Transition::ON_TO_ON | Transition::OFF_TO_OFF);

  return transition;
}

Transition TransitionFor(const LifeState &state, std::pair<int, int> cell) {
  bool next = state.StepFor(cell);
  switch (state.Get(cell) << 1 | next) {
  case 0b00: return Transition::OFF_TO_OFF;
  case 0b01: return Transition::OFF_TO_ON;
  case 0b10: return Transition::ON_TO_OFF;
  case 0b11: return Transition::ON_TO_ON;
  default: return Transition::IMPOSSIBLE;
  }
}

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
    return ::TransitionFor(state, cell);
  }

  // Would this transition be an interaction with the active pattern?
  bool TransitionIsPerturbation(std::pair<int, int> cell, Transition transition) const {
    return transition != Transition::STABLE_TO_STABLE && transition != UnperturbedTransitionFor(cell);
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
  LifeUnknownState result;

  LifeState activeUnknownZOI = (unknown & ~unknownStable).ZOI();

  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);

  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  // TODO: These could be cached in LifeStableState, as before
  LifeState stable3(false), stable2(false), stable1(false), stable0(false);
  CountNeighbourhood(stable.state, stable3, stable2, stable1, stable0);

#pragma clang loop vectorize_width(4)
  for (int i = 0; i < N; i++) {
    uint64_t on3 = state3[i];
    uint64_t on2 = state2[i];
    uint64_t on1 = state1[i];
    uint64_t on0 = state0[i];

    uint64_t unk3 = unknown3[i];
    uint64_t unk2 = unknown2[i];
    uint64_t unk1 = unknown1[i];
    uint64_t unk0 = unknown0[i];

    uint64_t current_on = state[i];
    uint64_t current_unknown = unknown[i];
    uint64_t stable_on = stable.state[i];
    uint64_t stable_unknown = stable.unknown[i];

    on2 |= on3;
    on1 |= on3;
    on0 |= on3;

    unk1 |= unk2 | unk3;
    unk0 |= unk2 | unk3;

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

    uint64_t s2 = stable2[i];
    uint64_t s1 = stable1[i];
    uint64_t s0 = stable0[i];

    uint64_t next_on = 0;
    uint64_t next_unknown = 0;
    uint64_t next_unknown_stable = 0;

    // Begin Autogenerated, see bitslicing/unknown_step_refined.py
#include "bitslicing/unknown_step_refined.hpp"
    // End Autogenerated

    // Handle the DONTCAREs
    next_on &= ~next_unknown;
    next_unknown_stable &= next_unknown;

    result.state[i] = (naive_next_on & activeUnknownZOI[i]) | (next_on & ~activeUnknownZOI[i]);
    result.unknown[i] = (naive_next_unknown & activeUnknownZOI[i]) | (next_unknown & ~activeUnknownZOI[i]);
    result.unknownStable[i] = (next_unknown_stable & ~activeUnknownZOI[i]);
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

  on2 |= on3;
  on1 |= on3;
  on0 |= on3;

  unk1 |= unk2 | unk3;
  unk0 |= unk2 | unk3;

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
  next_on &= ~next_unknown;
  next_unknown_stable &= next_unknown;

  uint64_t activeUnknownZOI = (unknown & ~unknownStable).ZOIColumn(i);

  uint64_t result_state = (naive_next_on & activeUnknownZOI) | (next_on & ~activeUnknownZOI);
  uint64_t result_unknown = (naive_next_unknown & activeUnknownZOI) | (next_unknown & ~activeUnknownZOI);
  uint64_t result_unknownStable = (next_unknown_stable & ~activeUnknownZOI);

  return {result_state, result_unknown, result_unknownStable};
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
