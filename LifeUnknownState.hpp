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

bool TransitionPrev(Transition t) {
  switch (t)   {
  case Transition::OFF_TO_OFF:
  case Transition::OFF_TO_ON:
    return false;

  case Transition::ON_TO_OFF:
  case Transition::ON_TO_ON:
    return true;

  case Transition::STABLE_TO_STABLE:
    return false;
  }
}

class LifeUnknownState {
public:
  LifeState state;
  LifeState unknown;
  LifeState unknownStable; // Equal to the stable state

  LifeUnknownState UncertainStepMaintaining(const LifeStableState &stable) const;
  // bool CanCleanlyAdvance(const LifeStableState &stable) const;
  LifeState ActiveComparedTo(const LifeStableState &stable) const;
  LifeState ChangesComparedTo(const LifeUnknownState &prev) const;
  void TransferStable(const LifeStableState &stable);

  void SetKnown(std::pair<int, int> cell, bool value, bool stable) {
    if (stable) {
      unknownStable.Set(cell);
    } else {
      state.Set(cell, value);
      unknown.Set(cell, false);
      unknownStable.Set(cell, false);
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
    }
  }

  bool StepFor(std::pair<int, int> cell) const {
    unsigned count = state.CountNeighbours(cell);
    if (state.Get(cell))
      return count == 2 || count == 3;
    else
      return count == 3;
  }

  Transition TransitionFor(std::pair<int, int> cell) const {
    bool next = StepFor(cell);
    switch (state.Get(cell) << 1 | next) {
    case 0b00: return Transition::OFF_TO_OFF;
    case 0b01: return Transition::OFF_TO_ON;
    case 0b10: return Transition::ON_TO_OFF;
    case 0b11: return Transition::ON_TO_ON;
    }
    std::cout << "Shouldn't" << std::endl;
  }

  // Would this transition be an interaction with the active pattern?
  bool TransitionIsPerturbation(std::pair<int, int> cell, Transition transition) const {
    return transition != Transition::STABLE_TO_STABLE && transition != TransitionFor(cell);
  }

  LifeHistoryState ToHistory() const {
    return LifeHistoryState(state, unknown, unknownStable, LifeState());
  }
};

LifeUnknownState LifeUnknownState::UncertainStepMaintaining(const LifeStableState &stable) const {
  LifeUnknownState result;

  LifeState activeUnknownZOI = (unknown & ~unknownStable).ZOI();
  // LifeState keepStable;

  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);

  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  // TODO: These could be cached in LifeStableState, as before
  LifeState stable3(false), stable2(false), stable1(false), stable0(false);
  CountNeighbourhood(stable.state, stable3, stable2, stable1, stable0);

  #pragma clang loop unroll(full)
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

    // ALWAYS CHECK THE PHASE that espresso outputs or you will get confused
    // Begin Autogenerated, see bitslicing/unknown_step.py
    naive_next_unknown |= current_on & (~on1) & (~on0) & (unk1 | unk0);
    naive_next_unknown |= (~on2) & unk1 & (on1 | on0 | unk0);
    naive_next_unknown |= (~on2) & on1 & unk0 & ~((current_unknown | current_on) & on0);
    naive_next_on |= (current_unknown | current_on | ~unk0) & (~on2) & on1 & on0 & (~unk1);
    naive_next_on |= current_on & (~on1) & (~on0) & (~unk1) & (~unk0);
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
next_on |= (~d2) & (~d4);
next_on |= (~current_on) & on2;
next_on |= on2 & on1;
next_on |= on2 & on0;
next_on |= (~d6) & (~s2) & (~s1);
next_on |= (~d6) & (~s2) & on1;
next_on |= (~d5) & (~s2) & (~s1) & (~s0);
next_on |= (~d5) & (~s2) & on1 & on0;
next_on |= (~d5) & (~current_on) & s0 & (~on0);
next_on |= s2 & (~on2) & (~on1) & (~on0);
next_on |= l2 & d1 & (~s2) & s0 & on2;
next_on |= d0 & d2 & (~s2) & (~s0) & on2;
next_on |= d2 & d6 & (~current_on) & (~s1) & (~s0) & (~on1);
next_on |= l2 & (~current_on) & (~s1) & s0 & (~on1) & on0;
next_on |= (~l2) & (~current_unknown) & s1 & s0 & (~on2) & (~on0);
next_on |= d4 & d5 & d6 & (~on2) & (~on1) & (~on0);
next_on |= d5 & d6 & s1 & (~on2) & (~on1) & (~on0);
next_on |= d6 & s1 & s0 & (~on2) & (~on1) & (~on0);
next_on |= d5 & d6 & (~current_unknown) & s1 & s0 & (~on2) & (~on1);
next_on |= d0 & d1 & d2 & (~s2) & (~s1) & on1 & on0;
next_on |= l2 & d1 & d5 & (~current_on) & s0 & on1 & on0;
next_on |= d0 & d2 & (~current_on) & (~s2) & (~s0) & on1 & on0;
next_on |= l3 & d5 & (~current_on) & s1 & s0 & (~on1) & on0;
next_on |= l2 & l3 & d4 & (~s1) & s0 & (~on1) & on0;
next_on |= l2 & d2 & d6 & (~s1) & (~s0) & (~on1) & on0;
next_on |= d1 & d2 & d4 & (~current_on) & (~s2) & (~s1) & (~on0);
next_on |= l2 & l3 & d1 & d5 & (~current_on) & (~s0) & (~on0);
next_on |= l2 & d2 & d6 & (~s1) & s0 & on1 & (~on0);
next_on |= l2 & d1 & (~current_on) & (~s2) & (~s0) & on1 & (~on0);
next_on |= l2 & l3 & d4 & s1 & (~s0) & on1 & (~on0);
next_on |= d1 & d4 & d5 & (~s1) & s0 & (~on1) & (~on0);
next_on |= l3 & d0 & d4 & (~s1) & (~s0) & (~on1) & (~on0);
next_on |= l2 & d0 & d1 & d2 & (~s2) & (~s0) & on1 & on0;
next_on |= d0 & d4 & (~current_on) & (~stable_on) & (~s1) & (~s0) & on1 & on0;
next_on |= l3 & d4 & d5 & (~current_unknown) & s1 & (~s0) & (~on1) & on0;
next_on |= l2 & l3 & d4 & (~current_on) & s1 & (~s0) & (~on1) & on0;
next_on |= l3 & d0 & d1 & d4 & d5 & (~s1) & (~s0) & on1 & on0;
next_on |= d1 & d2 & d5 & d6 & (~current_unknown) & (~s1) & (~s0) & on1 & (~on0);
next_unknown |= d4 & d5 & d6 & current_on & s0 & on0;
next_unknown |= d0 & d4 & d5 & d6 & current_on & (~s0) & (~on0);
next_unknown |= (~current_on) & (~stable_unknown) & on2;
next_unknown |= (~stable_unknown) & on2 & on1;
next_unknown |= (~stable_unknown) & on2 & on0;
next_unknown |= s2 & (~on2) & (~on1) & (~on0);
next_unknown |= d4 & current_on & s0 & (~on1) & on0;
next_unknown |= d0 & d1 & (~stable_unknown) & (~s2) & (~s1) & on2;
next_unknown |= d0 & (~stable_unknown) & (~s2) & (~s1) & (~s0) & on2;
next_unknown |= (~current_on) & (~stable_unknown) & (~s1) & (~s0) & (~on1) & (~on0);
next_unknown |= l3 & d0 & d1 & d2 & d4 & d5 & (~stable_unknown);
next_unknown |= l2 & d0 & d1 & d2 & d4 & d6 & (~stable_unknown);
next_unknown |= l3 & d0 & d1 & d2 & d5 & d6 & (~stable_unknown);
next_unknown |= l3 & d0 & d1 & d4 & d5 & d6 & (~stable_unknown);
next_unknown |= l3 & d0 & d2 & d4 & d5 & d6 & (~stable_unknown);
next_unknown |= l3 & d0 & d1 & d2 & (~stable_unknown) & (~s2) & on2;
next_unknown |= l3 & d2 & d4 & (~current_on) & (~stable_unknown) & s1 & on1;
next_unknown |= l3 & d4 & (~current_on) & (~stable_unknown) & (~s1) & s0 & (~on1);
next_unknown |= d2 & d6 & (~current_on) & (~stable_unknown) & (~s1) & (~s0) & (~on1);
next_unknown |= l3 & d4 & d5 & (~stable_unknown) & s0 & (~on2) & (~on1);
next_unknown |= d4 & d6 & (~current_unknown) & s1 & s0 & (~on2) & (~on1);
next_unknown |= l2 & d2 & d4 & d5 & d6 & current_on & on0;
next_unknown |= l3 & d1 & d5 & (~current_on) & (~stable_unknown) & s0 & on0;
next_unknown |= l3 & d2 & d4 & (~stable_unknown) & s1 & on1 & on0;
next_unknown |= d1 & d2 & current_on & (~s1) & s0 & on1 & on0;
next_unknown |= l3 & d2 & (~stable_unknown) & s1 & (~s0) & on1 & on0;
next_unknown |= l3 & d4 & d5 & (~stable_unknown) & s1 & (~on1) & on0;
next_unknown |= l3 & (~current_on) & (~stable_unknown) & (~s1) & s0 & (~on1) & on0;
next_unknown |= l3 & d1 & d2 & d4 & d5 & (~stable_unknown) & (~on0);
next_unknown |= l3 & d1 & d2 & d6 & current_on & s0 & (~on0);
next_unknown |= d2 & d4 & d5 & current_unknown & (~stable_unknown) & s0 & (~on0);
next_unknown |= d0 & d1 & d2 & d4 & current_on & (~s0) & (~on0);
next_unknown |= l3 & d1 & d5 & (~current_on) & (~stable_unknown) & (~s0) & (~on0);
next_unknown |= l3 & d4 & (~stable_unknown) & s1 & (~s0) & on1 & (~on0);
next_unknown |= l2 & d6 & (~stable_unknown) & s1 & s0 & (~on1) & (~on0);
next_unknown |= d5 & d6 & (~stable_unknown) & s1 & (~on2) & (~on1) & (~on0);
next_unknown |= l3 & d0 & d1 & d2 & d6 & current_on & (~s0) & on0;
next_unknown |= l3 & d1 & (~current_on) & (~stable_unknown) & (~s2) & s0 & on1 & on0;
next_unknown |= d0 & d2 & d4 & (~current_on) & (~stable_unknown) & (~s0) & on1 & on0;
next_unknown |= d0 & d1 & (~stable_unknown) & (~s2) & (~s1) & (~s0) & on1 & on0;
next_unknown |= d0 & (~current_on) & (~stable_unknown) & (~s2) & (~s1) & (~s0) & on1 & on0;
next_unknown |= l3 & d4 & (~current_on) & (~stable_unknown) & s1 & (~s0) & (~on1) & on0;
next_unknown |= l3 & d2 & (~stable_unknown) & (~s2) & (~s1) & (~s0) & (~on1) & on0;
next_unknown |= l3 & d1 & d2 & (~stable_unknown) & (~s2) & (~s1) & on1 & (~on0);
next_unknown |= l3 & d2 & (~stable_unknown) & (~s2) & (~s1) & s0 & on1 & (~on0);
next_unknown |= l3 & d1 & (~current_on) & (~stable_unknown) & (~s2) & (~s0) & on1 & (~on0);
next_unknown |= l2 & l3 & d4 & (~s1) & (~s0) & (~on2) & (~on1) & (~on0);
next_unknown |= d1 & d2 & d4 & d5 & d6 & (~current_on) & (~stable_unknown) & (~s1) & on1;
next_unknown |= d0 & d1 & d4 & d5 & d6 & current_unknown & (~stable_unknown) & (~s0) & on0;
next_unknown_stable |= stable_unknown & (~s1) & (~s0) & (~on2) & (~on1) & (~on0);
next_unknown_stable |= l3 & d2 & (~current_on) & s1 & (~s0) & (~on2) & on1;
next_unknown_stable |= (~current_on) & (~stable_on) & s1 & s0 & (~on2) & on1 & on0;
next_unknown_stable |= (~current_on) & (~stable_on) & (~s1) & s0 & (~on2) & (~on1) & on0;
next_unknown_stable |= d4 & (~current_on) & (~stable_on) & s1 & (~on2) & on1 & (~on0);
next_unknown_stable |= (~current_on) & (~stable_on) & s1 & (~s0) & (~on2) & on1 & (~on0);
next_unknown_stable |= l2 & d4 & stable_unknown & (~s1) & (~on2) & (~on1) & (~on0);
next_unknown_stable |= l3 & d2 & (~current_on) & (~s2) & (~s1) & (~on2) & (~on1) & on0;
next_unknown_stable |= l2 & d4 & stable_unknown & s1 & (~s0) & (~on2) & (~on1) & on0;
next_unknown_stable |= l3 & d2 & (~s2) & (~s1) & s0 & (~on2) & on1 & (~on0);
    // End Autogenerated

    // Correct the phase
    next_on = ~next_on;
    next_unknown = ~next_unknown;

    // Handle the DONTCAREs
    next_on &= ~next_unknown;
    next_unknown_stable &= next_unknown;

    result.state[i] = (naive_next_on & activeUnknownZOI[i]) | (next_on & ~activeUnknownZOI[i]);
    result.unknown[i] = (naive_next_unknown & activeUnknownZOI[i]) | (next_unknown & ~activeUnknownZOI[i]);
    result.unknownStable[i] = (next_unknown_stable & ~activeUnknownZOI[i]);
  }

  return result;
}

LifeState LifeUnknownState::ActiveComparedTo(const LifeStableState &stable) const {
  return ~unknown & ~stable.unknown & stable.stateZOI & (stable.state ^ state);
}

void LifeUnknownState::TransferStable(const LifeStableState &stable) {
  LifeState updated = unknownStable & ~stable.unknown;
  state |= stable.state & updated;
  unknown &= ~updated;
  unknownStable &= ~updated;
}

LifeState
LifeUnknownState::ChangesComparedTo(const LifeUnknownState &prev) const {
  return (state ^ prev.state) & ~unknown & ~prev.unknown;
}
