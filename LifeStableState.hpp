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

constexpr inline StableOptions operator~ (StableOptions a) { return static_cast<StableOptions>( ~static_cast<std::underlying_type<StableOptions>::type>(a) ); }
constexpr inline StableOptions operator| (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) | static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions operator& (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) & static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions operator^ (StableOptions a, StableOptions b) { return static_cast<StableOptions>( static_cast<std::underlying_type<StableOptions>::type>(a) ^ static_cast<std::underlying_type<StableOptions>::type>(b) ); }
constexpr inline StableOptions& operator|= (StableOptions& a, StableOptions b) { a = a | b; return a; }
constexpr inline StableOptions& operator&= (StableOptions& a, StableOptions b) { a = a & b; return a; }
constexpr inline StableOptions& operator^= (StableOptions& a, StableOptions b) { a = a ^ b; return a; }

StableOptions StableOptionsForCounts(unsigned countmask) {
  StableOptions result = StableOptions::IMPOSSIBLE;
  if ((1 << 2) & countmask) result |= StableOptions::LIVE2;
  if ((1 << 3) & countmask) result |= StableOptions::LIVE3;
  if ((1 << 0) & countmask) result |= StableOptions::DEAD0;
  if ((1 << 1) & countmask) result |= StableOptions::DEAD1;
  if ((1 << 2) & countmask) result |= StableOptions::DEAD2;
  if ((1 << 4) & countmask) result |= StableOptions::DEAD4;
  if ((1 << 5) & countmask) result |= StableOptions::DEAD5;
  if ((1 << 6) & countmask) result |= StableOptions::DEAD6;
  return result;
}

struct PropagateResult {
  bool consistent;
  bool changed;
};

class LifeStableState {
public:
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

  // This is a superset of state.ZOI(), but may have more cells
  //
  // If the active pattern is perturbed somewhere, we know the cell
  // must be in state.ZOI() eventually even if we don't know exactly
  // which cell is on
  LifeState stateZOI;

  // These are cached values are determined by the live/dead option masks
  LifeState state;
  LifeState unknown;

  bool operator==(const LifeStableState&) const = default;

  StableOptions GetOptions(std::pair<int, int> cell) const;
  void RestrictOptions(std::pair<int, int> cell, StableOptions options);

  bool SingletonOptions(std::pair<int, int> cell) {
    unsigned char bits = static_cast<unsigned char>(GetOptions(cell));
    return bits && !(bits & (bits-1));
  }
  
  void SetOn(const LifeState &state);
  void SetOff(const LifeState &state);
  void SetOn(std::pair<int, int> cell);
  void SetOff(std::pair<int, int> cell);

  // Propagate just using state/unknown and not the counts
  PropagateResult PropagateSimpleStep();
  PropagateResult PropagateSimple();

  PropagateResult StabiliseOptions();
  void UpdateStateKnown(std::pair<int, int> cell);
  PropagateResult SynchroniseStateKnown();
  PropagateResult UpdateOptions(); // Assumes counts and state/unknown are in sync
  PropagateResult SignalNeighbours(); // Assumes counts and state/unknown are in sync
  PropagateResult PropagateStep();
  PropagateResult Propagate();

  LifeState PerturbedUnknowns() const {
    LifeState perturbed = live2 | live3 | dead0 | dead1 | dead2 | dead4 | dead5 | dead6;
    return perturbed & unknown;
  }
  LifeState Vulnerable() const;

  PropagateResult TestUnknowns(const LifeState &cells);
  bool CompleteStableStep(std::chrono::system_clock::time_point &timeLimit, bool minimise, unsigned &maxPop, LifeState &best);
  LifeState CompleteStable(unsigned timeout, bool minimise);

  std::string RLE() const;
  std::string RLEWHeader() const {
    return "x = 0, y = 0, rule = LifeBellman\n" + RLE();
  }

  void SanityCheck() {
#ifdef DEBUG
    LifeStableState copy = *this;
    auto [consistent, changes] = copy.SynchroniseStateKnown();
    assert(consistent);
    assert(!changes);
#endif
  }
};

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
  LifeState exactlyOne = dead0;
  LifeState alreadySeen = dead0;
  exactlyOne = (alreadySeen & exactlyOne & ~dead1) | (~alreadySeen & ~exactlyOne & dead1); alreadySeen |= dead1;
  exactlyOne = (alreadySeen & exactlyOne & ~dead2) | (~alreadySeen & ~exactlyOne & dead2); alreadySeen |= dead2;
  exactlyOne = (alreadySeen & exactlyOne & ~dead4) | (~alreadySeen & ~exactlyOne & dead4); alreadySeen |= dead4;
  exactlyOne = (alreadySeen & exactlyOne & ~dead5) | (~alreadySeen & ~exactlyOne & dead5); alreadySeen |= dead5;
  exactlyOne = (alreadySeen & exactlyOne & ~dead6) | (~alreadySeen & ~exactlyOne & dead6);
  return exactlyOne & (live2 ^ live3);

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
{ uint64_t temp = (~stateunk) & (~stateon) & (~on2) & on1 & on0 & (~unk1) & unk0; signal_off |= temp; signal_on |= temp; abort |= temp; }
signal_on |= stateon & (~on2) & (~on1) & (~unk0);
set_off |= (~on1) & (~on0) & (~unk0);
{ uint64_t temp = stateon & (~on2) & (~on0) & (~unk1) & unk0; signal_off |= temp; signal_on |= temp; abort |= temp; }
{ uint64_t temp = stateon & (~on2) & on1 & on0; signal_off |= temp; abort |= temp; }
signal_off |= (~unk1) & (~unk0);
abort |= (~on1) & (~on0);
{ uint64_t temp = (~stateon) & (~on0); set_on |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & (~on1) & (~unk1); set_off |= temp; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = (~on2) & unk1; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & on2; set_off |= temp; set_on |= temp; signal_off |= temp; abort |= temp; }
{ uint64_t temp = stateunk; signal_off |= temp; abort |= temp; }
set_on = ~set_on;
signal_off = ~signal_off;
abort = ~abort;
// set_off |= on2 ;
// set_off |= (~on1) & ((~unk1) | ((~on0) & (~unk0)));
// set_on |= (~on2) & on1 & on0 & (~unk1) ;
// abort |= stateon & on2 & (on1 | on0) ;
// abort |= stateon & (~on1) & on0 & (~unk1) ;
// abort |= on1 & (~unk1) & (~unk0) & (((~stateon) & (~on2) & on0) | (stateon & (~on0))) ;
// signal_off |= (~stateunk) & (~stateon) & (~on2) & on1 & (~on0) & (~unk1) & unk0 ;
// signal_off |= stateon & (~on1) & (((~on0) & unk1) | ((~unk1) & unk0));
// signal_on |= (~stateunk) & (~stateon) & (~on2) & on1 & on0 & (~unk1) ;
// signal_on |= stateon & on1 & (~on0) & (~unk1) ;
// signal_on |= stateon & (~on1) & on0 & (~unk0) ;
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

void LifeStableState::UpdateStateKnown(std::pair<int, int> cell) {
  bool maybeLive = !(live2.Get(cell) && live3.Get(cell));
  bool maybeDead = !(dead0.Get(cell) && dead1.Get(cell) && dead2.Get(cell) && dead4.Get(cell) && dead5.Get(cell) && dead6.Get(cell));
  state.Set(cell, maybeLive && !maybeDead);
  unknown.Set(cell, maybeLive && maybeDead);

  if (maybeLive && !maybeDead) {
    stateZOI |= LifeState::CellZOI(cell);
  }
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
d4 |= stateunk & (~on2) & (~on1) & (~on0) & unk2 & (~unk1) & (~unk0);
d5 |= stateunk & (~on2) & (~on1) & unk2 & (~unk1) & (~unk0);
d5 |= stateunk & (~on2) & (~on1) & (~on0) & unk2 & (~unk1);
{ uint64_t temp = stateunk & (~on1) & (~on0) & (~unk2) & unk1 & (~unk0); l2 |= temp; d2 |= temp; }
d5 |= (~on2) & (~on1) & (~on0) & unk2 & (~unk1) & (~unk0);
{ uint64_t temp = stateunk & on0 & (~unk2) & (~unk1) & unk0; l2 |= temp; d2 |= temp; d4 |= temp; }
{ uint64_t temp = (~stateon) & (~on1) & (~unk3) & (~unk2) & (~unk1) & (~unk0); d2 |= temp; d6 |= temp; abort |= temp; }
{ uint64_t temp = (~stateon) & (~on2) & on0 & (~unk2) & (~unk1) & unk0; d5 |= temp; d6 |= temp; abort |= temp; }
d6 |= stateunk & (~on1) & on0 & (~unk1) & unk0;
d4 |= stateunk & (~on1) & on0 & (~unk2) & unk0;
d6 |= stateunk & (~on1) & (~on0) & unk1 & (~unk0);
d6 |= stateunk & on1 & (~on0) & (~unk1) & (~unk0);
{ uint64_t temp = (~on2) & (~on0) & (~unk3) & (~unk2) & (~unk1) & unk0; l3 |= temp; d4 |= temp; d5 |= temp; abort |= temp; }
d6 |= (~on2) & (~on1) & unk2 & (~unk1) & (~unk0);
d4 |= stateunk & on1 & (~on0) & (~unk2) & (~unk0);
{ uint64_t temp = (~on1) & on0 & (~unk2) & (~unk1) & unk0; l3 |= temp; d4 |= temp; }
d6 |= (~on2) & (~on1) & (~on0) & unk2 & (~unk1);
d5 |= stateunk & (~on2) & (~unk2) & unk1 & (~unk0);
{ uint64_t temp = stateunk & (~on0) & (~unk3) & (~unk2) & (~unk1); d1 |= temp; d5 |= temp; }
{ uint64_t temp = (~on0) & (~unk3) & (~unk2) & (~unk1) & (~unk0); d1 |= temp; d5 |= temp; }
{ uint64_t temp = (~stateon) & (~on2) & on1 & (~on0) & (~unk2); d6 |= temp; abort |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~unk2) & unk1 & (~unk0); l3 |= temp; d4 |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~on0) & (~unk2) & unk1; l3 |= temp; d4 |= temp; }
{ uint64_t temp = (~on1) & (~on0) & (~unk3) & (~unk2) & (~unk1); l2 |= temp; d2 |= temp; d6 |= temp; }
{ uint64_t temp = (~on2) & (~unk3) & (~unk2) & (~unk1) & (~unk0); l3 |= temp; d4 |= temp; d5 |= temp; }
abort |= stateon & on1 & on0;
d5 |= stateunk & on1 & (~on0) & (~unk2);
d6 |= (~on2) & (~unk2) & unk1 & (~unk0);
d5 |= on1 & (~on0) & (~unk2) & (~unk0);
d6 |= stateunk & (~on2) & on1 & (~unk2);
{ uint64_t temp = (~stateon) & on1 & on0; l2 |= temp; d2 |= temp; }
{ uint64_t temp = (~on2) & (~on1) & (~unk2) & unk1; d5 |= temp; d6 |= temp; }
{ uint64_t temp = on2 & (~on1) & (~on0); l2 |= temp; d0 |= temp; abort |= temp; }
{ uint64_t temp = (~stateunk) & (~stateon); l2 |= temp; l3 |= temp; }
d4 |= on2 & on0;
abort |= (~on2) & unk2;
abort |= (~on2) & unk1;
{ uint64_t temp = on2 & on1; d4 |= temp; d5 |= temp; }
{ uint64_t temp = (~stateon) & on2; l2 |= temp; l3 |= temp; d1 |= temp; d2 |= temp; abort |= temp; }
abort |= unk3;
d0 |= on0;
{ uint64_t temp = on1; d0 |= temp; d1 |= temp; }
{ uint64_t temp = stateon; d1 |= temp; d2 |= temp; d4 |= temp; d5 |= temp; d6 |= temp; }
abort = ~abort;
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

    uint64_t signaloff = 0;
    uint64_t signalon = 0;

// Begin Autogenerated, see bitslicing/stable_count.py
signalon |= (~l2) & d0 & d1 & (~s1) & (~s0) & (~unk2) & unk0;
signalon |= d0 & d1 & d2 & (~s2) & (~s1) & (~s0) & unk2 & (~unk1) & (~unk0);
signalon |= d1 & d2 & d4 & (~d5) & (~s2) & (~s1) & unk2 & (~unk1) & (~unk0);
signalon |= d2 & d4 & d5 & (~d6) & s1 & (~s0) & (~unk1) & (~unk0);
signalon |= d0 & d1 & d2 & d4 & (~s2) & (~s1) & (~s0) & (~unk1) & unk0;
signaloff |= s2 & s1;
signalon |= l2 & l3 & d4 & s1 & s0 & (~unk2) & (~unk0);
signalon |= d0 & d1 & (~d2) & (~s1) & (~s0) & (~unk2) & unk1 & (~unk0);
signalon |= d4 & d5 & (~d6) & (~s2) & (~unk2) & unk0;
signalon |= d2 & d4 & (~d5) & s1 & (~s0) & (~unk2);
signalon |= (~l2) & d1 & (~s1) & (~unk2) & unk1 & (~unk0);
signaloff |= (~l2) & l3 & s1 & s0;
signalon |= d1 & d2 & d4 & d5 & (~d6) & (~s1) & s0 & (~unk1) & unk0;
signalon |= d0 & d1 & d2 & d4 & d5 & (~d6) & (~s1) & (~s0) & unk1 & (~unk0);
signalon |= l2 & d2 & (~s2) & s1 & (~s0) & (~unk2) & (~unk0);
signalon |= l2 & d1 & d2 & (~s2) & (~s1) & s0 & (~unk2) & unk0;
signalon |= l2 & d1 & d5 & s0 & (~unk2) & (~unk1) & unk0;
signalon |= l3 & d0 & d2 & d4 & d6 & (~s0) & (~unk2) & (~unk1) & unk0;
signaloff |= l2 & l3 & d2 & d4 & d5 & d6 & s0;
signaloff |= l2 & l3 & d1 & d2 & d4 & d5 & d6;
signaloff |= d6 & s2 & s0;
signaloff |= l3 & (~d2) & d4 & d5 & d6 & s1;
signaloff |= d5 & d6 & s2;
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

PropagateResult LifeStableState::PropagateStep() {
  bool changedEver = false;

  auto result = StabiliseOptions();
  if (!result.consistent)
    return {false, false};
  changedEver = result.changed;

  PropagateResult signalresult = SignalNeighbours();
  if (!signalresult.consistent)
    return {false, false};
  changedEver = changedEver || signalresult.changed;

  return {true, changedEver};
}

PropagateResult LifeStableState::Propagate() {
  auto [consistent, changedEver] = PropagateSimple();
  if (!consistent)
    return {false, false};

  bool done = false;
  while (!done) {
    auto result = PropagateStep();
    if (!result.consistent)
      return {false, false};
    if (result.changed)
      changedEver = true;
    done = !result.changed;
  }

  // Need to do one last options update after the last SignalNeighbours
  if (changedEver) {
    auto result = StabiliseOptions();
    if (!result.consistent)
      return {false, false};
  }

  return {true, changedEver};
}


PropagateResult LifeStableState::TestUnknowns(const LifeState &cells) {
  // Try all the nearby changes to see if any are forced
  LifeState remainingCells = cells;
  bool change = false;
  while (!remainingCells.IsEmpty()) {
    auto cell = remainingCells.FirstOn();
    remainingCells.Erase(cell);

    // Try on
    LifeStableState onSearch = *this;
    onSearch.SetOn(cell);
    // auto onResult = onSearch.PropagateColumn(cell.first);
    auto onResult = onSearch.Propagate();

    // Try off
    LifeStableState offSearch = *this;
    offSearch.SetOff(cell);
    // auto offResult = offSearch.PropagateColumn(cell.first);
    auto offResult = offSearch.Propagate();

    if (!onResult.consistent && !offResult.consistent) {
      return {false, false};
    }

    if (onResult.consistent && !offResult.consistent) {
      *this = onSearch;
      change = true;
    }

    if (!onResult.consistent && offResult.consistent) {
      *this = offSearch;
      change = true;
    }

    if (onResult.consistent && offResult.consistent && onResult.changed && offResult.changed) {
      // Copy over common cells
      LifeState agreement = unknown & ~onSearch.unknown & ~offSearch.unknown & ~(onSearch.state ^ offSearch.state);
      if (!agreement.IsEmpty()) {
        // std::cout << "Agreement:" << std::endl;
        // std::cout << LifeHistoryState(state | (agreement & onSearch.state), unknown & ~agreement, agreement).RLEWHeader() << std::endl;
        SetOn(agreement & onSearch.state);
        SetOff(agreement & ~onSearch.state);
        change = true;
      }
    }

    remainingCells &= unknown;
  }

  if (change)
    return {Propagate().consistent, true};
  else
    return {true, false};
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

  LifeState unknown3(false), unknown2(false), unknown1(false), unknown0(false);
  CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);

  auto result = TestUnknowns(stateZOI & unknown);
  if (!result.consistent)
    return false;

  if (result.changed) {
    currentPop = state.GetPop();
    if(currentPop >= maxPop)
      return false;
    CountNeighbourhood(unknown, unknown3, unknown2, unknown1, unknown0);
  }

  LifeState settable = PerturbedUnknowns();

  if (settable.IsEmpty()) {
    // We win
    best = state;
    maxPop = state.GetPop();
    return true;
  }

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


std::string LifeStableState::RLE() const {
  LifeState marked = unknown | state;
  return LifeBellmanRLEFor(state, marked);
}
