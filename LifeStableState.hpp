#pragma once

#include "LifeAPI.h"
#include "Bits.hpp"
#include "Parsing.hpp"

#include "LifeHistoryState.hpp"

enum class StableOptions : unsigned char {
  LIVE2 = 1 << 0,
  LIVE3 = 1 << 1,
  DEAD0 = 1 << 2,
  DEAD1 = 1 << 3,
  DEAD2 = 1 << 4,
  DEAD4 = 1 << 5,
  DEAD5 = 1 << 6,
  DEAD6 = 1 << 7,

  IMPOSSIBLE = 0,
  LIVE = LIVE2 | LIVE3,
  DEAD = DEAD0 | DEAD1 | DEAD2 | DEAD4 | DEAD5 | DEAD6,
};

StableOptions StableOptionsHighest(StableOptions t) {
  if (t == StableOptions::IMPOSSIBLE) return StableOptions::IMPOSSIBLE;
  unsigned int bits = static_cast<unsigned int>(t);
  return static_cast<StableOptions>(static_cast<unsigned char>(1 << (8*sizeof(bits)-1-__builtin_clz(bits))));
}

constexpr inline StableOptions operator~ (StableOptions a) { return static_cast<StableOptions>( ~static_cast<std::underlying_type<StableOptions>::type>(a) ); }
constexpr inline StableOptions operator| (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) | static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions operator& (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) & static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions operator^ (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) ^ static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions& operator|= (StableOptions& a, StableOptions b) { a = a | b; return a; }
constexpr inline StableOptions& operator&= (StableOptions& a, StableOptions b) { a = a & b; return a; }
constexpr inline StableOptions& operator^= (StableOptions& a, StableOptions b) { a = a ^ b; return a; }

struct PropagateResult {
  bool consistent;
  bool changed;
};

class LifeStableState {
public:
  LifeState state;
  LifeState unknown;

  // This is a superset of state.ZOI(), but may have more cells
  //
  // If the active pattern is perturbed somewhere, we know the cell
  // must be in state.ZOI() eventually even if we don't know exactly
  // which cell is on
  LifeState stateZOI;

  // IMPORTANT: These are stored flipped to the meaning of StableOptions, so
  // 0 is possible, 1 is ruled out
  LifeState live2;
  LifeState live3;
  LifeState dead0;
  LifeState dead1;
  LifeState dead2;
  LifeState dead4;
  LifeState dead5;
  LifeState dead6;

  bool operator==(const LifeStableState&) const = default;

  LifeStableState Join(const LifeStableState &other) const;

  StableOptions GetOptions(std::pair<int, int> cell) const;
  void RestrictOptions(std::pair<int, int> cell, StableOptions options);

  bool SingletonOptions(std::pair<int, int> cell) {
    unsigned char bits = static_cast<unsigned char>(GetOptions(cell));
    return bits && !(bits & (bits-1));
  }
  
  void SetOn(const LifeState &state);
  void SetOff(const LifeState &state);
  void SetOn(int column, uint64_t which);
  void SetOff(int column, uint64_t which);
  void SetOn(std::pair<int, int> cell);
  void SetOff(std::pair<int, int> cell);

  // Propagate just using state/unknown and not the counts
  PropagateResult PropagateSimpleStep();
  PropagateResult PropagateSimple();

  PropagateResult SynchroniseStateKnown();
  PropagateResult UpdateOptions(); // Assumes counts and state/unknown are in sync
  PropagateResult StabiliseOptions(); // Apply the above two repeatedly

  PropagateResult SignalNeighbours(); // Assumes counts and state/unknown are in sync
  PropagateResult PropagateStep();
  PropagateResult Propagate();

  PropagateResult SynchroniseStateKnownColumn(int column);

  PropagateResult PropagateSimpleStepStrip(int column);
  PropagateResult PropagateSimpleStrip(int column);

  PropagateResult SynchroniseStateKnownStrip(int column);
  PropagateResult UpdateOptionsStrip(int column); // Assumes counts and state/unknown are in sync
  PropagateResult StabiliseOptionsStrip(int column);
  PropagateResult SignalNeighboursStrip(int column); // Assumes counts and state/unknown are in sync
  PropagateResult PropagateStepStrip(int column);
  PropagateResult PropagateStrip(int column);

  PropagateResult SynchroniseStateKnown(std::pair<int, int> cell);

  LifeState PerturbedUnknowns() const {
    LifeState perturbed = live2 | live3 | dead0 | dead1 | dead2 | dead4 | dead5 | dead6;
    return perturbed & unknown;
  }
  LifeState Vulnerable() const;

  PropagateResult TestUnknown(std::pair<int, int> cell);
  PropagateResult TestUnknowns(const LifeState &cells);
  bool CompleteStableStep(std::chrono::system_clock::time_point &timeLimit, bool minimise, unsigned &maxPop, LifeState &best);
  LifeState CompleteStable(unsigned timeout, bool minimise);

  std::string RLE() const;
  std::string RLEWHeader() const {
    return "x = 0, y = 0, rule = LifeBellman\n" + RLE();
  }

  bool CompatibleWith(const LifeState &desired) const;
  bool CompatibleWith(const LifeStableState &desired) const;

  void SanityCheck() {
#ifdef DEBUG
    LifeStableState copy = *this;
    auto [consistent, changes] = copy.SynchroniseStateKnown();
    assert(consistent);
    assert(!changes);
#endif
  }
};

LifeStableState LifeStableState::Join(const LifeStableState &other) const {
  LifeStableState result;

  result.unknown = unknown | other.unknown | (state ^ other.state);
  result.state = state & ~result.unknown;
  result.stateZOI = stateZOI & other.stateZOI;

  result.live2 = live2 & other.live2;
  result.live3 = live3 & other.live3;
  result.dead0 = dead0 & other.dead0;
  result.dead1 = dead1 & other.dead1;
  result.dead2 = dead2 & other.dead2;
  result.dead4 = dead4 & other.dead4;
  result.dead5 = dead5 & other.dead5;
  result.dead6 = dead6 & other.dead6;

  return result;
}

StableOptions LifeStableState::GetOptions(std::pair<int, int> cell) const {
  auto result = StableOptions::IMPOSSIBLE;
  if(!live2.Get(cell)) result |= StableOptions::LIVE2;
  if(!live3.Get(cell)) result |= StableOptions::LIVE3;
  if(!dead0.Get(cell)) result |= StableOptions::DEAD0;
  if(!dead1.Get(cell)) result |= StableOptions::DEAD1;
  if(!dead2.Get(cell)) result |= StableOptions::DEAD2;
  if(!dead4.Get(cell)) result |= StableOptions::DEAD4;
  if(!dead5.Get(cell)) result |= StableOptions::DEAD5;
  if(!dead6.Get(cell)) result |= StableOptions::DEAD6;
  return result;
}
void LifeStableState::RestrictOptions(std::pair<int, int> cell,
                                      StableOptions options) {
  if ((options & StableOptions::LIVE2) != StableOptions::LIVE2) live2.Set(cell);
  if ((options & StableOptions::LIVE3) != StableOptions::LIVE3) live3.Set(cell);
  if ((options & StableOptions::DEAD0) != StableOptions::DEAD0) dead0.Set(cell);
  if ((options & StableOptions::DEAD1) != StableOptions::DEAD1) dead1.Set(cell);
  if ((options & StableOptions::DEAD2) != StableOptions::DEAD2) dead2.Set(cell);
  if ((options & StableOptions::DEAD4) != StableOptions::DEAD4) dead4.Set(cell);
  if ((options & StableOptions::DEAD5) != StableOptions::DEAD5) dead5.Set(cell);
  if ((options & StableOptions::DEAD6) != StableOptions::DEAD6) dead6.Set(cell);
}

void LifeStableState::SetOn(const LifeState &which) {
  state |= which;
  stateZOI |= which.ZOI();
  unknown &= ~which;
  dead0 |= which;
  dead1 |= which;
  dead2 |= which;
  dead4 |= which;
  dead5 |= which;
  dead6 |= which;
}
void LifeStableState::SetOff(const LifeState &which) {
  state &= ~which;
  unknown &= ~which;
  live2 |= which;
  live3 |= which;
}

void LifeStableState::SetOn(int i, uint64_t which) {
  state[i] |= which;
  stateZOI |= LifeState::ColumnZOI(i, which);
  unknown[i] &= ~which;
  dead0[i] |= which;
  dead1[i] |= which;
  dead2[i] |= which;
  dead4[i] |= which;
  dead5[i] |= which;
  dead6[i] |= which;
}
void LifeStableState::SetOff(int i, uint64_t which) {
  state[i] &= ~which;
  unknown[i] &= ~which;
  live2[i] |= which;
  live3[i] |= which;
}

void LifeStableState::SetOn(std::pair<int, int> cell) {
  unknown.Erase(cell);
  state.Set(cell);
  stateZOI |= LifeState::CellZOI(cell);
  RestrictOptions(cell, StableOptions::LIVE);
}

void LifeStableState::SetOff(std::pair<int, int> cell) {
  unknown.Erase(cell);
  state.Erase(cell);
  RestrictOptions(cell, StableOptions::DEAD);
}

LifeState LifeStableState::Vulnerable() const {
  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  LifeState new_vulnerable(false);

  for (int i = 0; i < N; i++) {
    uint64_t l2 = live2[i];
    uint64_t l3 = live3[i];
    uint64_t d0 = dead0[i];
    uint64_t d1 = dead1[i];
    uint64_t d2 = dead2[i];
    uint64_t d4 = dead4[i];
    uint64_t d5 = dead5[i];
    uint64_t d6 = dead6[i];

    uint64_t s2 = state2[i];
    uint64_t s1 = state1[i];
    uint64_t s0 = state0[i];

    uint64_t unk3 = unknown3[i];
    uint64_t unk2 = unknown2[i];
    uint64_t unk1 = unknown1[i];
    uint64_t unk0 = unknown0[i];

    uint64_t vulnerable = 0;

    // Begin Autogenerated
#include "bitslicing/stable_vulnerable.hpp"
    // End Autogenerated

    new_vulnerable[i] = vulnerable;
  }

  return new_vulnerable;
}

PropagateResult LifeStableState::PropagateSimpleStep() {
  LifeState startUnknown = unknown;

  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  LifeState new_off(false), new_on(false), new_signal_off(false), new_signal_on(false);

  uint64_t has_set_off = 0;
  uint64_t has_set_on = 0;
  uint64_t has_signal_off = 0;
  uint64_t has_signal_on = 0;
  uint64_t has_abort = 0;

  for (int i = 0; i < N; i++) {
    uint64_t on2 = state2[i];
    uint64_t on1 = state1[i];
    uint64_t on0 = state0[i];

    uint64_t unk3 = unknown3[i];
    uint64_t unk2 = unknown2[i];
    uint64_t unk1 = unknown1[i];
    uint64_t unk0 = unknown0[i];

    unk1 |= unk2 | unk3;
    unk0 |= unk2 | unk3;

    uint64_t stateon = state[i];
    uint64_t stateunk = unknown[i];

    // These are the 5 output bits that are calculated for each cell
    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
    uint64_t signal_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

    // Begin Autogenerated
#include "bitslicing/stable_simple.hpp"
    // End Autogenerated

   signal_off &= unk0 | unk1;
   signal_on  &= unk0 | unk1;

   new_off[i] = set_off & stateunk;
   new_on[i] = set_on & stateunk;
   new_signal_off[i] = signal_off;
   new_signal_on[i] = signal_on;

   has_set_off |= set_off;
   has_set_on |= set_on;
   has_signal_off |= signal_off;
   has_signal_on |= signal_on;
   has_abort |= abort;
  }

  if(has_abort != 0)
    return {false, false};

  if (has_set_on != 0) {
    state |= new_on;
    unknown &= ~new_on;
  }

  if (has_set_off != 0) {
    unknown &= ~new_off;
  }

  LifeState off_zoi(false);
  LifeState on_zoi(false);
  if (has_signal_off != 0) {
    off_zoi = new_signal_off.ZOI();
    unknown &= ~off_zoi;
  }

  if (has_signal_on != 0) {
    on_zoi = new_signal_on.ZOI();
    state |= on_zoi & unknown;
    unknown &= ~on_zoi;
  }

  if (has_signal_on != 0 && has_signal_off != 0) {
    if (!(on_zoi & off_zoi & unknown).IsEmpty()) {
      return {false, false};
    }
  }

  return {true, unknown != startUnknown};
}

PropagateResult LifeStableState::PropagateSimple() {
  bool done = false;
  bool changed = false;
  while (!done) {
    auto result = PropagateSimpleStep();
    if (!result.consistent)
      return {false, false};
    if (result.changed)
      changed = true;
    done = !result.changed;
  }

  stateZOI |= state.ZOI();

  if (changed) {
    auto result = StabiliseOptions();
    if (!result.consistent)
      return {false, false};
  }

  return {true, changed};
}

PropagateResult LifeStableState::SynchroniseStateKnown() {
  LifeState knownOn = ~unknown & state;
  dead0 |= knownOn;
  dead1 |= knownOn;
  dead2 |= knownOn;
  dead4 |= knownOn;
  dead5 |= knownOn;
  dead6 |= knownOn;

  LifeState knownOff = ~unknown & ~state;
  live2 |= knownOff;
  live3 |= knownOff;

  LifeState maybeLive = ~(live2 & live3);
  LifeState maybeDead = ~(dead0 & dead1 & dead2 & dead4 & dead5 & dead6);

  LifeState impossibles = ~maybeLive & ~maybeDead;
  if (!impossibles.IsEmpty())
    return {false, false};

  LifeState changes;
  changes |= ~state & (maybeLive & ~maybeDead);
  state = maybeLive & ~maybeDead;

  changes |= ~stateZOI & state.ZOI();
  stateZOI |= state.ZOI();

  changes |= ~unknown & (maybeLive & maybeDead);
  unknown = maybeLive & maybeDead;

  return {true, !changes.IsEmpty()};
}

PropagateResult LifeStableState::UpdateOptions() {
  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  uint64_t has_abort = 0;
  uint64_t changes = 0;

  for (int i = 0; i < N; i++) {
    uint64_t on2 = state2[i];
    uint64_t on1 = state1[i];
    uint64_t on0 = state0[i];

    uint64_t unk3 = unknown3[i];
    uint64_t unk2 = unknown2[i];
    uint64_t unk1 = unknown1[i];
    uint64_t unk0 = unknown0[i];

    uint64_t stateon = state[i];
    uint64_t stateunk = unknown[i];

    uint64_t abort = 0;

    uint64_t l2 = 0;
    uint64_t l3 = 0;
    uint64_t d0 = 0;
    uint64_t d1 = 0;
    uint64_t d2 = 0;
    uint64_t d4 = 0;
    uint64_t d5 = 0;
    uint64_t d6 = 0;

// Begin Autogenerated, see bitslicing/stable_count.py
#include "bitslicing/stable_count.hpp"
// End Autogenerated

    changes |= l2 & ~live2[i];
    changes |= l3 & ~live3[i];
    changes |= d0 & ~dead0[i];
    changes |= d1 & ~dead1[i];
    changes |= d2 & ~dead2[i];
    changes |= d4 & ~dead4[i];
    changes |= d5 & ~dead5[i];
    changes |= d6 & ~dead6[i];

    live2[i] |= l2;
    live3[i] |= l3;
    dead0[i] |= d0;
    dead1[i] |= d1;
    dead2[i] |= d2;
    dead4[i] |= d4;
    dead5[i] |= d5;
    dead6[i] |= d6;
    has_abort |= abort;
  }

  return {has_abort == 0, changes != 0};
}

PropagateResult LifeStableState::SignalNeighbours() {
  LifeState state3(false), state2(false), state1(false), state0(false);
  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(state, state3, state2, state1, state0);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  LifeState new_signal_off(false), new_signal_on(false);

  for (int i = 0; i < N; i++) {
    uint64_t l2 = live2[i];
    uint64_t l3 = live3[i];
    uint64_t d0 = dead0[i];
    uint64_t d1 = dead1[i];
    uint64_t d2 = dead2[i];
    uint64_t d4 = dead4[i];
    uint64_t d5 = dead5[i];
    uint64_t d6 = dead6[i];

    uint64_t s2 = state2[i];
    uint64_t s1 = state1[i];
    uint64_t s0 = state0[i];

    uint64_t unk3 = unknown3[i];
    uint64_t unk2 = unknown2[i];
    uint64_t unk1 = unknown1[i];
    uint64_t unk0 = unknown0[i];

    uint64_t stateon = state[i];
    uint64_t stateunk = unknown[i];

    uint64_t signaloff = 0;
    uint64_t signalon = 0;

// Begin Autogenerated, see bitslicing/stable_signal.py
#include "bitslicing/stable_signal.hpp"
// End Autogenerated

    new_signal_off[i] = signaloff;
    new_signal_on[i] = signalon;
  }

  LifeState off_zoi = new_signal_off.ZOI() & ~new_signal_off;
  LifeState on_zoi = new_signal_on.ZOI() & ~new_signal_on;

  if (!(off_zoi & on_zoi & unknown).IsEmpty())
    return {false, false};

  LifeState changes = (off_zoi & unknown) | (on_zoi & unknown);

  SetOff(off_zoi & unknown);
  SetOn(on_zoi & unknown);

  return {true, !changes.IsEmpty()};
}

PropagateResult LifeStableState::StabiliseOptions() {
  bool changedEver = false;
  bool done = false;
  while (!done) {
    done = false;

    PropagateResult knownresult = SynchroniseStateKnown();
    if (!knownresult.consistent)
      return {false, false};

    PropagateResult optionsresult = UpdateOptions();
    if (!optionsresult.consistent)
      return {false, false};

    done = !optionsresult.changed && !knownresult.changed;
    changedEver = changedEver || !done;
  }
  return {true, changedEver};
}

PropagateResult LifeStableState::Propagate() {
  auto [consistent, changedEver] = PropagateSimple();
  if (!consistent)
    return {false, false};

  bool done = false;
  while (!done) {
    done = false;

    PropagateResult knownresult = SynchroniseStateKnown();
    if (!knownresult.consistent)
      return {false, false};

    PropagateResult optionsresult = UpdateOptions();
    if (!optionsresult.consistent)
      return {false, false};

    PropagateResult signalresult = SignalNeighbours();
    if (!signalresult.consistent)
      return {false, false};

    done = !optionsresult.changed && !knownresult.changed && !signalresult.changed;
    changedEver = changedEver || !done;
  }
  return {true, changedEver};
}

PropagateResult LifeStableState::PropagateSimpleStepStrip(int column) {
  std::array<uint64_t, 6> nearbyState;
  std::array<uint64_t, 6> nearbyUnknown;

  for (int i = 0; i < 6; i++) {
    int c = (column + i - 2 + N) % N;
    nearbyState[i] = state[c];
    nearbyUnknown[i] = unknown[c];
  }

  std::array<uint64_t, 4> state3, state2, state1, state0;
  std::array<uint64_t, 4> unknown3, unknown2, unknown1, unknown0;
  CountNeighbourhoodStrip(nearbyState, state3, state2, state1, state0);
  CountNeighbourhoodStrip(nearbyUnknown, unknown3, unknown2, unknown1, unknown0);

  std::array<uint64_t, 6> new_off;
  std::array<uint64_t, 6> new_on;

  std::array<uint64_t, 6> new_signal_off {0};
  std::array<uint64_t, 6> new_signal_on {0};

  uint64_t has_abort = 0;

  #pragma clang loop vectorize(enable)
  for (int i = 1; i < 5; i++) {
    uint64_t on3 = state3[i-1];
    uint64_t on2 = state2[i-1];
    uint64_t on1 = state1[i-1];
    uint64_t on0 = state0[i-1];
    on2 |= on3;
    on1 |= on3;
    on0 |= on3;

    uint64_t unk3 = unknown3[i-1];
    uint64_t unk2 = unknown2[i-1];
    uint64_t unk1 = unknown1[i-1];
    uint64_t unk0 = unknown0[i-1];

    unk1 |= unk2 | unk3;
    unk0 |= unk2 | unk3;

    uint64_t stateon = nearbyState[i];
    uint64_t stateunk = nearbyUnknown[i];

    // These are the 5 output bits that are calculated for each cell
    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
    uint64_t signal_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

    // Begin Autogenerated
#include "bitslicing/stable_simple.hpp"
    // End Autogenerated

    signal_off &= unk0 | unk1;
    signal_on  &= unk0 | unk1;

    new_off[i] = set_off & stateunk;
    new_on[i] = set_on & stateunk;
    new_signal_off[i] = signal_off;
    new_signal_on[i] = signal_on;

    has_abort |= abort;
  }

  if(has_abort != 0)
    return {false, false};

  std::array<uint64_t, 6> signalled_off {0};
  std::array<uint64_t, 6> signalled_on {0};

  #pragma clang loop vectorize(enable)
  for (int i = 1; i < 5; i++) {
   uint64_t smear_off = RotateLeft(new_signal_off[i]) | new_signal_off[i] | RotateRight(new_signal_off[i]);
   signalled_off[i-1] |= smear_off;
   signalled_off[i]   |= smear_off;
   signalled_off[i+1] |= smear_off;

   uint64_t smear_on  = RotateLeft(new_signal_on[i])  | new_signal_on[i]  | RotateRight(new_signal_on[i]);
   signalled_on[i-1] |= smear_on;
   signalled_on[i]   |= smear_on;
   signalled_on[i+1] |= smear_on;
  }

  for (int i = 1; i < 5; i++) {
   signalled_off[i] &= ~new_signal_off[i];
   signalled_on[i] &= ~new_signal_on[i];
  }

  uint64_t signalled_overlaps = 0;
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    signalled_overlaps |= nearbyUnknown[i] & signalled_off[i] & signalled_on[i];
  }
  if(signalled_overlaps != 0)
    return {false, false};

  #pragma clang loop vectorize(enable)
  for (int i = 1; i < 5; i++) {
    int orig = (column + i - 2 + N) % N;
    state[orig]  |= new_on[i];
    unknown[orig] &= ~new_off[i];
    unknown[orig] &= ~new_on[i];
  }

  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    int orig = (column + i - 2 + N) % N;
    state[orig]  |= signalled_on[i] & nearbyUnknown[i];
    unknown[orig] &= ~signalled_on[i];
    unknown[orig] &= ~signalled_off[i];
  }

  uint64_t unknownChanges = 0;
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    int orig = (column + i - 2 + N) % N;
    unknownChanges |= unknown[orig] ^ nearbyUnknown[i];
  }

  return { true, unknownChanges != 0 };
}


PropagateResult LifeStableState::SynchroniseStateKnownColumn(int i) {
  uint64_t knownOn = ~unknown[i] & state[i];
  dead0[i] |= knownOn;
  dead1[i] |= knownOn;
  dead2[i] |= knownOn;
  dead4[i] |= knownOn;
  dead5[i] |= knownOn;
  dead6[i] |= knownOn;

  uint64_t knownOff = ~unknown[i] & ~state[i];
  live2[i] |= knownOff;
  live3[i] |= knownOff;

  uint64_t maybeLive = ~(live2[i] & live3[i]);
  uint64_t maybeDead = ~(dead0[i] & dead1[i] & dead2[i] & dead4[i] & dead5[i] & dead6[i]);

  uint64_t impossibles = ~maybeLive & ~maybeDead;
  if (impossibles != 0)
    return {false, false};

  uint64_t changes = 0;
  changes |= ~state[i] & (maybeLive & ~maybeDead);
  state[i] = maybeLive & ~maybeDead;

  changes |= ~stateZOI[i] & state.ZOIColumn(i);
  stateZOI[i] |= state.ZOIColumn(i);

  changes |= ~unknown[i] & (maybeLive & maybeDead);
  unknown[i] = maybeLive & maybeDead;

  return {true, changes != 0};
}

PropagateResult LifeStableState::SynchroniseStateKnownStrip(int column) {
  // TODO: check this optimises
  bool anyChanges = false;
  for (int i = 0; i < 4; i++) {
    int c = (column + i - 2 + N) % N;
    auto result = SynchroniseStateKnownColumn(c);
    if(!result.consistent)
      return {false, false};
    anyChanges = anyChanges || result.changed;
  }
  return {true, anyChanges};
}

PropagateResult LifeStableState::SynchroniseStateKnown(std::pair<int, int> cell) {
  bool knownOn = !unknown.Get(cell) && state.Get(cell);
  if(knownOn) {
    dead0.Set(cell);
    dead1.Set(cell);
    dead2.Set(cell);
    dead4.Set(cell);
    dead5.Set(cell);
    dead6.Set(cell);
  }
  bool knownOff = !unknown.Get(cell) && !state.Get(cell);
  if (knownOff) {
    live2.Set(cell);
    live3.Set(cell);
  }

  bool maybeLive = !(live2.Get(cell) && live3.Get(cell));
  bool maybeDead = !(dead0.Get(cell) && dead1.Get(cell) && dead2.Get(cell) && dead4.Get(cell) && dead5.Get(cell) && dead6.Get(cell));
  state.Set(cell, maybeLive && !maybeDead);
  unknown.Set(cell, maybeLive && maybeDead);

  if (maybeLive && !maybeDead) {
    stateZOI |= LifeState::CellZOI(cell);
  }

  return {true, false};
}

PropagateResult LifeStableState::PropagateSimpleStrip(int column) {
  bool done = false;
  bool changed = false;
  while (!done) {
    auto result = PropagateSimpleStepStrip(column);
    if (!result.consistent)
      return {false, false};
    changed = changed || result.changed;
    done = !result.changed;
  }

  stateZOI |= state.ZOI(); // TODO!

  if (changed) {
    auto result = StabiliseOptionsStrip(column);
    if (!result.consistent)
      return {false, false};
  }

  return {true, changed};
}

PropagateResult LifeStableState::SignalNeighboursStrip(int column) {
  std::array<uint64_t, 6> nearbyState;
  std::array<uint64_t, 6> nearbyUnknown;

  for (int i = 0; i < 6; i++) {
    int c = (column + i - 2 + N) % N;
    nearbyState[i] = state[c];
    nearbyUnknown[i] = unknown[c];
  }

  std::array<uint64_t, 4> nearbylive2, nearbylive3,
      nearbydead0, nearbydead1, nearbydead2, nearbydead4, nearbydead5, nearbydead6;

  for (int i = 1; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    nearbylive2[i-1] = live2[c];
    nearbylive3[i-1] = live3[c];
    nearbydead0[i-1] = dead0[c];
    nearbydead1[i-1] = dead1[c];
    nearbydead2[i-1] = dead2[c];
    nearbydead4[i-1] = dead4[c];
    nearbydead5[i-1] = dead5[c];
    nearbydead6[i-1] = dead6[c];
  }

  std::array<uint64_t, 4> state3, state2, state1, state0;
  std::array<uint64_t, 4> unknown3, unknown2, unknown1, unknown0;
  CountNeighbourhoodStrip(nearbyState, state3, state2, state1, state0);
  CountNeighbourhoodStrip(nearbyUnknown, unknown3, unknown2, unknown1, unknown0);

  std::array<uint64_t, 6> new_signal_off, new_signal_on;

  for (int i = 1; i < 5; i++) {
    int orig = (column + i - 2 + N) % N;

    uint64_t l2 = nearbylive2[i-1];
    uint64_t l3 = nearbylive3[i-1];
    uint64_t d0 = nearbydead0[i-1];
    uint64_t d1 = nearbydead1[i-1];
    uint64_t d2 = nearbydead2[i-1];
    uint64_t d4 = nearbydead4[i-1];
    uint64_t d5 = nearbydead5[i-1];
    uint64_t d6 = nearbydead6[i-1];

    uint64_t s2 = state2[i-1];
    uint64_t s1 = state1[i-1];
    uint64_t s0 = state0[i-1];

    uint64_t unk3 = unknown3[i-1];
    uint64_t unk2 = unknown2[i-1];
    uint64_t unk1 = unknown1[i-1];
    uint64_t unk0 = unknown0[i-1];

    uint64_t stateon = nearbyState[i];
    uint64_t stateunk = nearbyUnknown[i];

    uint64_t signaloff = 0;
    uint64_t signalon = 0;

// Begin Autogenerated, see bitslicing/stable_signal.py
#include "bitslicing/stable_signal.hpp"
// End Autogenerated

    new_signal_off[i] = signaloff;
    new_signal_on[i] = signalon;
  }

  std::array<uint64_t, 6> signalled_off {0};
  std::array<uint64_t, 6> signalled_on {0};

  #pragma clang loop vectorize(enable)
  for (int i = 1; i < 5; i++) {
   uint64_t smear_off = RotateLeft(new_signal_off[i]) | new_signal_off[i] | RotateRight(new_signal_off[i]);
   signalled_off[i-1] |= smear_off;
   signalled_off[i]   |= smear_off;
   signalled_off[i+1] |= smear_off;

   uint64_t smear_on  = RotateLeft(new_signal_on[i])  | new_signal_on[i]  | RotateRight(new_signal_on[i]);
   signalled_on[i-1] |= smear_on;
   signalled_on[i]   |= smear_on;
   signalled_on[i+1] |= smear_on;
  }
  for (int i = 1; i < 5; i++) {
   signalled_off[i] &= ~new_signal_off[i];
   signalled_on[i] &= ~new_signal_on[i];
  }

  uint64_t signalled_overlaps = 0;
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    signalled_overlaps |= nearbyUnknown[i] & signalled_off[i] & signalled_on[i];
  }
  if(signalled_overlaps != 0)
    return {false, false};

  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    int orig = (column + i - 2 + N) % N;
    SetOff(orig, signalled_off[i] & nearbyUnknown[i]);
    SetOn( orig, signalled_on[i]  & nearbyUnknown[i]);
  }

  uint64_t unknownChanges = 0;
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < 6; i++) {
    unknownChanges |= (signalled_on[i] & nearbyUnknown[i]) | (signalled_off[i] & nearbyUnknown[i]);
  }

  return {true, unknownChanges != 0};
}

PropagateResult LifeStableState::UpdateOptionsStrip(int column) {
  std::array<uint64_t, 6> nearbyState;
  std::array<uint64_t, 6> nearbyUnknown;

  for (int i = 0; i < 6; i++) {
    int c = (column + i - 2 + N) % N;
    nearbyState[i] = state[c];
    nearbyUnknown[i] = unknown[c];
  }

  std::array<uint64_t, 4> nearbylive2, nearbylive3,
      nearbydead0, nearbydead1, nearbydead2, nearbydead4, nearbydead5, nearbydead6;

  for (int i = 1; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    nearbylive2[i-1] = live2[c];
    nearbylive3[i-1] = live3[c];
    nearbydead0[i-1] = dead0[c];
    nearbydead1[i-1] = dead1[c];
    nearbydead2[i-1] = dead2[c];
    nearbydead4[i-1] = dead4[c];
    nearbydead5[i-1] = dead5[c];
    nearbydead6[i-1] = dead6[c];
  }

  std::array<uint64_t, 4> state3, state2, state1, state0;
  std::array<uint64_t, 4> unknown3, unknown2, unknown1, unknown0;
  CountNeighbourhoodStrip(nearbyState, state3, state2, state1, state0);
  CountNeighbourhoodStrip(nearbyUnknown, unknown3, unknown2, unknown1, unknown0);

  uint64_t has_abort = 0;
  uint64_t changes = 0;

  for (int i = 1; i < 5; i++) {
    uint64_t on2 = state2[i-1];
    uint64_t on1 = state1[i-1];
    uint64_t on0 = state0[i-1];

    uint64_t unk3 = unknown3[i-1];
    uint64_t unk2 = unknown2[i-1];
    uint64_t unk1 = unknown1[i-1];
    uint64_t unk0 = unknown0[i-1];

    uint64_t stateon = nearbyState[i];
    uint64_t stateunk = nearbyUnknown[i];

    uint64_t abort = 0;

    uint64_t l2 = 0;
    uint64_t l3 = 0;
    uint64_t d0 = 0;
    uint64_t d1 = 0;
    uint64_t d2 = 0;
    uint64_t d4 = 0;
    uint64_t d5 = 0;
    uint64_t d6 = 0;

// Begin Autogenerated, see bitslicing/stable_count.py
#include "bitslicing/stable_count.hpp"
// End Autogenerated

    int orig = (column + i - 2 + N) % N;

    changes |= l2 & ~nearbylive2[i-1];
    changes |= l3 & ~nearbylive3[i-1];
    changes |= d0 & ~nearbydead0[i-1];
    changes |= d1 & ~nearbydead1[i-1];
    changes |= d2 & ~nearbydead2[i-1];
    changes |= d4 & ~nearbydead4[i-1];
    changes |= d5 & ~nearbydead5[i-1];
    changes |= d6 & ~nearbydead6[i-1];

    nearbylive2[i-1] |= l2;
    nearbylive3[i-1] |= l3;
    nearbydead0[i-1] |= d0;
    nearbydead1[i-1] |= d1;
    nearbydead2[i-1] |= d2;
    nearbydead4[i-1] |= d4;
    nearbydead5[i-1] |= d5;
    nearbydead6[i-1] |= d6;
    has_abort |= abort;
  }

  for (int i = 1; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    live2[c] = nearbylive2[i-1];
    live3[c] = nearbylive3[i-1];
    dead0[c] = nearbydead0[i-1];
    dead1[c] = nearbydead1[i-1];
    dead2[c] = nearbydead2[i-1];
    dead4[c] = nearbydead4[i-1];
    dead5[c] = nearbydead5[i-1];
    dead6[c] = nearbydead6[i-1];
  }

  return {has_abort == 0, changes != 0};
}

PropagateResult LifeStableState::StabiliseOptionsStrip(int column) {
  bool changedEver = false;
  bool done = false;
  while (!done) {
    done = false;

    PropagateResult knownresult = SynchroniseStateKnownStrip(column);
    if (!knownresult.consistent)
      return {false, false};

    PropagateResult optionsresult = UpdateOptionsStrip(column);
    if (!optionsresult.consistent)
      return {false, false};

    done = !optionsresult.changed && !knownresult.changed;
    changedEver = changedEver || !done;
  }
  return {true, changedEver};
}

PropagateResult LifeStableState::PropagateStrip(int column) {
  auto [consistent, changedEver] = PropagateSimpleStrip(column);
  if (!consistent)
    return {false, false};

  bool done = false;
  while (!done) {
    done = false;

    PropagateResult knownresult = SynchroniseStateKnownStrip(column);
    if (!knownresult.consistent)
      return {false, false};

    PropagateResult optionsresult = UpdateOptionsStrip(column);
    if (!optionsresult.consistent)
      return {false, false};

    PropagateResult signalresult = SignalNeighboursStrip(column);
    if (!signalresult.consistent)
      return {false, false};

    done = !optionsresult.changed && !knownresult.changed && !signalresult.changed;
    changedEver = changedEver || !done;
  }
  return {true, changedEver};
}

PropagateResult LifeStableState::TestUnknown(std::pair<int, int> cell) {
  // Try on
  LifeStableState onSearch = *this;
  onSearch.SetOn(cell);
  auto onResult = onSearch.PropagateStrip(cell.first);

  // Try off
  LifeStableState offSearch = *this;
  offSearch.SetOff(cell);
  auto offResult = offSearch.PropagateStrip(cell.first);

  if (!onResult.consistent && !offResult.consistent)
    return {false, false};

  bool change = false;

  if (onResult.consistent && !offResult.consistent) {
    *this = onSearch;
    change = true;
  }

  if (!onResult.consistent && offResult.consistent) {
    *this = offSearch;
    change = true;
  }

  if (onResult.consistent && offResult.consistent && onResult.changed && offResult.changed) {
    LifeStableState joined = onSearch.Join(offSearch);
    change = joined != *this;
    *this = joined;
  }

  return {true, change};
}


PropagateResult LifeStableState::TestUnknowns(const LifeState &cells) {
  // Try all the nearby changes to see if any are forced
  LifeState remainingCells = cells & unknown;
  bool anyChanges = false;
  while (!remainingCells.IsEmpty()) {
    auto cell = remainingCells.FirstOn();
    remainingCells.Erase(cell);

    LifeStableState copy = *this;
    auto result = TestUnknown(cell);
    if (!result.consistent)
      return {false, false};
    anyChanges = anyChanges || result.changed;

    remainingCells &= unknown;
  }

  auto result = StabiliseOptions();
  if (!result.consistent)
    return {false, false};
  anyChanges = anyChanges || result.changed;

  return {true, anyChanges};
}

bool LifeStableState::CompleteStableStep(std::chrono::system_clock::time_point &timeLimit, bool minimise, unsigned &maxPop, LifeState &best) {
  auto currentTime = std::chrono::system_clock::now();
  if(currentTime > timeLimit)
    return false;

  bool consistent = Propagate().consistent;
  if (!consistent)
    return false;

  unsigned currentPop = state.GetPop();

  if (currentPop >= maxPop) {
    return false;
  }

  auto result = TestUnknowns(Vulnerable().ZOI());
  if (!result.consistent)
    return false;

  if (result.changed) {
    currentPop = state.GetPop();
    if(currentPop >= maxPop)
      return false;
  }

  LifeState settable = PerturbedUnknowns();

  if (settable.IsEmpty()) {
    // We win
    best = state;
    maxPop = state.GetPop();
    return true;
  }

  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  // Now make a guess
  std::pair<int, int> newPlacement = {-1, -1};

  newPlacement = (settable & (~unknown3 & ~unknown2 & unknown1 & ~unknown0)).FirstOn();
  if(newPlacement.first == -1)
    newPlacement = (settable & (~unknown3 & ~unknown2 & unknown1 & unknown0)).FirstOn();
  if(newPlacement.first == -1)
    newPlacement = settable.FirstOn();
  if(newPlacement.first == -1)
    return false;

  bool onresult = false;
  bool offresult = false;

  // Try off
  {
    LifeStableState nextState = *this;
    nextState.SetOff(LifeState::Cell(newPlacement));
    offresult = nextState.CompleteStableStep(timeLimit, minimise, maxPop, best);
  }
  if (!minimise && offresult)
    return true;

  // Then must be on
  {
    LifeStableState &nextState = *this;
    nextState.SetOn(LifeState::Cell(newPlacement));

    // if (currentPop == maxPop - 2) {
    //   // All remaining unknown cells must be off
    //   nextState.unknownStable = LifeState();
    // }

    onresult = nextState.CompleteStableStep(timeLimit, minimise, maxPop, best);
  }

  return offresult || onresult;
}

LifeState LifeStableState::CompleteStable(unsigned timeout, bool minimise) {
  if (unknown.IsEmpty()) {
    return state;
  }

  LifeState best;
  unsigned maxPop = std::numeric_limits<int>::max();
  LifeState searchArea = state;

  auto startTime = std::chrono::system_clock::now();
  auto timeLimit = startTime + std::chrono::seconds(timeout);

  while(!(unknown & ~searchArea).IsEmpty()) {
    searchArea = searchArea.ZOI();
    LifeStableState copy = *this;
    copy.unknown &= searchArea;
    copy.CompleteStableStep(timeLimit, minimise, maxPop, best);

    auto currentTime = std::chrono::system_clock::now();
    if (best.GetPop() > 0 || currentTime > timeLimit)
      break;
  }
  return best;
}


bool LifeStableState::CompatibleWith(const LifeState &desired) const {
  LifeStableState desiredStable = LifeStableState();
  desiredStable.state = desired;
  desiredStable.StabiliseOptions();
  return CompatibleWith(desiredStable);
}

bool LifeStableState::CompatibleWith(const LifeStableState &desired) const {
  if(!(live2 & ~desired.live2).IsEmpty()) return false;
  if(!(live3 & ~desired.live3).IsEmpty()) return false;
  if(!(dead0 & ~desired.dead0).IsEmpty()) return false;
  if(!(dead1 & ~desired.dead1).IsEmpty()) return false;
  if(!(dead2 & ~desired.dead2).IsEmpty()) return false;
  if(!(dead4 & ~desired.dead4).IsEmpty()) return false;
  if(!(dead5 & ~desired.dead5).IsEmpty()) return false;
  if(!(dead6 & ~desired.dead6).IsEmpty()) return false;
  if(!(~unknown & ~desired.unknown & (state ^ desired.state)).IsEmpty()) return false;
  return true;
}

std::string LifeStableState::RLE() const {
  LifeState marked = unknown | state;
  return LifeBellmanRLEFor(state, marked);
}
