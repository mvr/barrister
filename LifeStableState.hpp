#pragma once

#include "LifeAPI.h"
#include "Bits.hpp"

class LifeStableState {
public:
  LifeState state;
  LifeState stateZOI;
  LifeState unknownStable;
  LifeState glanced; // Glanced cells are OFF cells that have at most one ON neighbour
  LifeState glancedON;  // GlancedON cells are OFF cells have at least two ON neighbours

  // Neighbour counts in binary
  LifeState state0;
  LifeState state1;
  LifeState state2;

  LifeState unknown0;
  LifeState unknown1;
  LifeState unknown2;
  LifeState unknown3;

  bool SimplePropagateColumnStep(int column); // NOTE: doesn't update the counts

  // std::pair<bool, bool> SimplePropagateStableStep();
  // bool SimplePropagateStable();
  std::pair<bool, bool> PropagateStableStep();
  bool PropagateStable();

  std::pair<int, int> UnknownNeighbour(std::pair<int, int> cell) const;

  // void SetCell(std::pair<int, int> cell, bool value) {
  //   // TODO, probably just need to re-count.
  // }

  bool TestUnknowns();
  bool CompleteStableStep(unsigned &maxPop, LifeState &best);
  LifeState CompleteStable();
};

bool LifeStableState::SimplePropagateColumnStep(int column) {
  std::array<uint64_t, 5> nearbyStable;
  std::array<uint64_t, 5> nearbyUnknown;

  for (int i = 0; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    nearbyStable[i] = state.state[c];
    nearbyUnknown[i] = unknownStable.state[c];
  }

  std::array<uint64_t, 5> oncol0;
  std::array<uint64_t, 5> oncol1;
  std::array<uint64_t, 5> unkcol0;
  std::array<uint64_t, 5> unkcol1;

  uint64_t has_abort = 0;

  for (int i = 0; i < 5; i++) {
    uint64_t a = nearbyStable[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    oncol0[i] = l ^ r ^ a;
    oncol1[i] = ((l ^ r) & a) | (l & r);
  }

  for (int i = 0; i < 5; i++) {
    uint64_t a = nearbyUnknown[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    unkcol0[i] = l ^ r ^ a;
    unkcol1[i] = ((l ^ r) & a) | (l & r);
  }

  std::array<uint64_t, 5> new_off;
  std::array<uint64_t, 5> new_on;

  std::array<uint64_t, 5> signalled_off {0};
  std::array<uint64_t, 5> signalled_on {0};

  for (int i = 1; i < 4; i++) {
    int idxU = i-1;
    int idxB = i+1;

    uint64_t on3, on2, on1, on0;
    uint64_t unk3, unk2, unk1, unk0;

    {
      uint64_t u_on1 = oncol1[idxU];
      uint64_t u_on0 = oncol0[idxU];
      uint64_t c_on1 = oncol1[i];
      uint64_t c_on0 = oncol0[i];
      uint64_t l_on1 = oncol1[idxB];
      uint64_t l_on0 = oncol0[idxB];

      uint64_t uc0, uc1, uc2, uc_carry0;
      HalfAdd(uc0, uc_carry0, u_on0, c_on0);
      FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

      uint64_t on_carry1, on_carry0;
      HalfAdd(on0, on_carry0, uc0, l_on0);
      FullAdd(on1, on_carry1, uc1, l_on1, on_carry0);
      HalfAdd(on2, on3, uc2, on_carry1);
      on2 |= on3;
      on1 |= on3;
      on0 |= on3;

      uint64_t u_unk1 = unkcol1[idxU];
      uint64_t u_unk0 = unkcol0[idxU];
      uint64_t c_unk1 = unkcol1[i];
      uint64_t c_unk0 = unkcol0[i];
      uint64_t l_unk1 = unkcol1[idxB];
      uint64_t l_unk0 = unkcol0[idxB];

      uint64_t ucunk0, ucunk1, ucunk2, ucunk_carry0;
      HalfAdd(ucunk0, ucunk_carry0, u_unk0, c_unk0);
      FullAdd(ucunk1, ucunk2, u_unk1, c_unk1, ucunk_carry0);

      uint64_t unk_carry1, unk_carry0;
      HalfAdd(unk0, unk_carry0, ucunk0, l_unk0);
      FullAdd(unk1, unk_carry1, ucunk1, l_unk1, unk_carry0);
      HalfAdd(unk2, unk3, ucunk2, unk_carry1);
      unk1 |= unk2 | unk3;
      unk0 |= unk2 | unk3;
    }

    uint64_t state0 = nearbyStable[i];
    uint64_t state1 = nearbyUnknown[i];

    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

    uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
    uint64_t signal_on = 0;

// Begin Autogenerated
set_off |= on2 ;
set_off |= (~on1) & ((~unk1) | ((~on0) & (~unk0)));
set_on |= (~on2) & on1 & on0 & (~unk1) ;
abort |= state0 & on2 & (on1 | on0) ;
abort |= state0 & (~on1) & on0 & (~unk1) ;
abort |= on1 & (~unk1) & (~unk0) & (((~state0) & (~on2) & on0) | (state0 & (~on0))) ;
signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk1) & unk0 ;
signal_off |= state0 & (~on1) & (((~on0) & unk1) | ((~unk1) & unk0));
signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk1) ;
signal_on |= state0 & on1 & (~on0) & (~unk1) ;
signal_on |= state0 & (~on1) & on0 & (~unk0) ;
// End Autogenerated

   new_off[i] = set_off & state1;
   new_on[i]  = set_on  & state1;

   uint64_t smear_off = RotateLeft(signal_off) | signal_off | RotateRight(signal_off);
   signalled_off[i-1] |= smear_off;
   signalled_off[i]   |= smear_off;
   signalled_off[i+1] |= smear_off;

   uint64_t smear_on  = RotateLeft(signal_on)  | signal_on  | RotateRight(signal_on);
   signalled_on[i-1] |= smear_on;
   signalled_on[i]   |= smear_on;
   signalled_on[i+1] |= smear_on;

   has_abort |= abort;
  }

  if(has_abort != 0)
    return false;

  uint64_t signalled_overlaps = 0;
  for (int i = 0; i < 5; i++) {
    signalled_overlaps |= nearbyUnknown[i] & signalled_off[i] & signalled_on[i];
  }
  if(signalled_overlaps != 0)
    return false;

  for (int i = 1; i < 4; i++) {
    int orig = (column + i - 2 + 64) % 64;
    state.state[orig]  |= new_on[i];
    unknownStable.state[orig] &= ~new_off[i];
    unknownStable.state[orig] &= ~new_on[i];
  }

  for (int i = 0; i < 5; i++) {
    int orig = (column + i - 2 + 64) % 64;
    state.state[orig]  |= signalled_on[i] & nearbyUnknown[i];
    unknownStable.state[orig] &= ~signalled_on[i];
    unknownStable.state[orig] &= ~signalled_off[i];
  }

  return true;
}

// std::pair<bool, bool> LifeStableState::SimplePropagateStableStep() {
//   LifeState startUnknownStable = unknownStable;

//   LifeState oncol0(false), oncol1(false), unkcol0(false), unkcol1(false);
//   CountRows(state, oncol0, oncol1);
//   CountRows(unknownStable, unkcol0, unkcol1);

//   LifeState new_off(false), new_on(false);

//   uint64_t has_set_off = 0;
//   uint64_t has_set_on = 0;
//   uint64_t has_abort = 0;

//   for (int i = 0; i < N; i++) {
//     int idxU;
//     int idxB;
//     if (i == 0)
//       idxU = N - 1;
//     else
//       idxU = i - 1;

//     if (i == N - 1)
//       idxB = 0;
//     else
//       idxB = i + 1;

//     // Sum up the number of on/unknown cells in the 3x3 square. This
//     // is done for a whole 64bit column at a time, with the 4bit
//     // result stored in four separate 64bit ints.

//     uint64_t on3, on2, on1, on0;
//     uint64_t unk3, unk2, unk1, unk0;

//     {
//       uint64_t u_on1 = oncol1.state[idxU];
//       uint64_t u_on0 = oncol0.state[idxU];
//       uint64_t c_on1 = oncol1.state[i];
//       uint64_t c_on0 = oncol0.state[i];
//       uint64_t l_on1 = oncol1.state[idxB];
//       uint64_t l_on0 = oncol0.state[idxB];

//       uint64_t uc0, uc1, uc2, uc_carry0;
//       HalfAdd(uc0, uc_carry0, u_on0, c_on0);
//       FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

//       uint64_t on_carry1, on_carry0;
//       HalfAdd(on0, on_carry0, uc0, l_on0);
//       FullAdd(on1, on_carry1, uc1, l_on1, on_carry0);
//       HalfAdd(on2, on3, uc2, on_carry1);
//       on2 |= on3;
//       on1 |= on3;
//       on0 |= on3;

//       uint64_t u_unk1 = unkcol1.state[idxU];
//       uint64_t u_unk0 = unkcol0.state[idxU];
//       uint64_t c_unk1 = unkcol1.state[i];
//       uint64_t c_unk0 = unkcol0.state[i];
//       uint64_t l_unk1 = unkcol1.state[idxB];
//       uint64_t l_unk0 = unkcol0.state[idxB];

//       uint64_t ucunk0, ucunk1, ucunk2, ucunk_carry0;
//       HalfAdd(ucunk0, ucunk_carry0, u_unk0, c_unk0);
//       FullAdd(ucunk1, ucunk2, u_unk1, c_unk1, ucunk_carry0);

//       uint64_t unk_carry1, unk_carry0;
//       HalfAdd(unk0, unk_carry0, ucunk0, l_unk0);
//       FullAdd(unk1, unk_carry1, ucunk1, l_unk1, unk_carry0);
//       HalfAdd(unk2, unk3, ucunk2, unk_carry1);
//       unk1 |= unk2 | unk3;
//       unk0 |= unk2 | unk3;
//     }

//     uint64_t state0 = state.state[i];
//     uint64_t state1 = unknownStable.state[i];

//     // These are the 5 output bits that are calculated for each cell
//     uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
//     uint64_t set_on = 0;
//     uint64_t abort = 0; // The neighbourhood is inconsistent

// // Begin Autogenerated
// set_off |= on2 ;
// set_off |= (~on1) & ((~unk1) | ((~on0) & (~unk0)));
// set_on |= (~on2) & on1 & on0 & (~unk1) ;
// abort |= state0 & on2 & (on1 | on0) ;
// abort |= state0 & (~on1) & on0 & (~unk1) ;
// abort |= on1 & (~unk1) & (~unk0) & (((~state0) & (~on2) & on0) | (state0 & (~on0))) ;
// // End Autogenerated

//    new_off.state[i] = set_off & state1;
//    new_on.state[i] = set_on & state1;

//    has_set_off |= set_off;
//    has_set_on |= set_on;
//    has_abort |= abort;
//   }

//   if(has_abort != 0)
//     return std::make_pair(false, false);

//   if (has_set_on != 0) {
//     state |= new_on;
//     unknownStable &= ~new_on;
//   }

//   if (has_set_off != 0) {
//     unknownStable &= ~new_off;
//   }

//   return std::make_pair(has_abort == 0, unknownStable == startUnknownStable);
// }

// bool LifeStableState::SimplePropagateStable() {
//   bool done = false;
//   while (!done) {
//     auto result = SimplePropagateStableStep();
//     if (!result.first) {
//       return false;
//     }
//     done = result.second;
//   }
//   return true;
// }

// std::pair<bool, bool> LifeStableState::PropagateStableStep() {
//   LifeState startUnknownStable = unknownStable;

//   LifeState oncol0(false), oncol1(false), unkcol0(false), unkcol1(false);
//   CountRows(state, oncol0, oncol1);
//   CountRows(unknownStable, unkcol0, unkcol1);

//   LifeState new_off(false), new_on(false), new_signal_off(false), new_signal_on(false);

//   uint64_t has_set_off = 0;
//   uint64_t has_set_on = 0;
//   uint64_t has_signal_off = 0;
//   uint64_t has_signal_on = 0;
//   uint64_t has_abort = 0;

//   for (int i = 0; i < N; i++) {
//     int idxU;
//     int idxB;
//     if (i == 0)
//       idxU = N - 1;
//     else
//       idxU = i - 1;

//     if (i == N - 1)
//       idxB = 0;
//     else
//       idxB = i + 1;

//     // Sum up the number of on/unknown cells in the 3x3 square. This
//     // is done for a whole 64bit column at a time, with the 4bit
//     // result stored in four separate 64bit ints.

//     uint64_t on3, on2, on1, on0;
//     uint64_t unk3, unk2, unk1, unk0;

//     {
//       uint64_t u_on1 = oncol1.state[idxU];
//       uint64_t u_on0 = oncol0.state[idxU];
//       uint64_t c_on1 = oncol1.state[i];
//       uint64_t c_on0 = oncol0.state[i];
//       uint64_t l_on1 = oncol1.state[idxB];
//       uint64_t l_on0 = oncol0.state[idxB];

//       uint64_t uc0, uc1, uc2, uc_carry0;
//       HalfAdd(uc0, uc_carry0, u_on0, c_on0);
//       FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

//       uint64_t on_carry1, on_carry0;
//       HalfAdd(on0, on_carry0, uc0, l_on0);
//       FullAdd(on1, on_carry1, uc1, l_on1, on_carry0);
//       HalfAdd(on2, on3, uc2, on_carry1);

//       on2 |= on3;
//       on1 |= on3;
//       on0 |= on3;

//       uint64_t u_unk1 = unkcol1.state[idxU];
//       uint64_t u_unk0 = unkcol0.state[idxU];
//       uint64_t c_unk1 = unkcol1.state[i];
//       uint64_t c_unk0 = unkcol0.state[i];
//       uint64_t l_unk1 = unkcol1.state[idxB];
//       uint64_t l_unk0 = unkcol0.state[idxB];

//       uint64_t ucunk0, ucunk1, ucunk2, ucunk_carry0;
//       HalfAdd(ucunk0, ucunk_carry0, u_unk0, c_unk0);
//       FullAdd(ucunk1, ucunk2, u_unk1, c_unk1, ucunk_carry0);

//       uint64_t unk_carry1, unk_carry0;
//       HalfAdd(unk0, unk_carry0, ucunk0, l_unk0);
//       FullAdd(unk1, unk_carry1, ucunk1, l_unk1, unk_carry0);
//       HalfAdd(unk2, unk3, ucunk2, unk_carry1);
//       unk1 |= unk2 | unk3;
//       unk0 |= unk2 | unk3;
//     }

//     uint64_t state0 = state.state[i];
//     uint64_t state1 = unknownStable.state[i];

//     // These are the 5 output bits that are calculated for each cell
//     uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
//     uint64_t set_on = 0;
//     uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
//     uint64_t signal_on = 0;
//     uint64_t abort = 0; // The neighbourhood is inconsistent

// // Begin Autogenerated
// set_off |= on2 ;
// set_off |= (~on1) & ((~unk1) | ((~on0) & (~unk0)));
// set_on |= (~on2) & on1 & on0 & (~unk1) ;
// abort |= state0 & on2 & (on1 | on0) ;
// abort |= state0 & (~on1) & on0 & (~unk1) ;
// abort |= on1 & (~unk1) & (~unk0) & (((~state0) & (~on2) & on0) | (state0 & (~on0))) ;
// signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk1) & unk0 ;
// signal_off |= state0 & (~on1) & (((~on0) & unk1) | ((~unk1) & unk0));
// signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk1) ;
// signal_on |= state0 & on1 & (~on0) & (~unk1) ;
// signal_on |= state0 & (~on1) & on0 & (~unk0) ;
// // End Autogenerated

//    new_off.state[i] = set_off & state1;
//    new_on.state[i] = set_on & state1;
//    new_signal_off.state[i] = signal_off;
//    new_signal_on.state[i] = signal_on;

//    has_set_off |= set_off;
//    has_set_on |= set_on;
//    has_signal_off |= signal_off;
//    has_signal_on |= signal_on;
//    has_abort |= abort;
//   }

//   if(has_abort != 0)
//     return std::make_pair(false, false);

//   if (has_set_on != 0) {
//     state |= new_on;
//     unknownStable &= ~new_on;
//   }

//   if (has_set_off != 0) {
//     unknownStable &= ~new_off;
//   }

//   LifeState off_zoi(false);
//   LifeState on_zoi(false);
//   if (has_signal_off != 0) {
//     off_zoi = new_signal_off.ZOI();
//     unknownStable &= ~off_zoi;
//   }

//   if (has_signal_on != 0) {
//     on_zoi = new_signal_on.ZOI();
//     state |= on_zoi & unknownStable;
//     unknownStable &= ~on_zoi;
//   }

//   if (has_signal_on != 0 && has_signal_off != 0) {
//     if(!(on_zoi & off_zoi & unknownStable).IsEmpty()) {
//       has_abort = 1;
//     }
//   }

//   return std::make_pair(has_abort == 0, unknownStable == startUnknownStable);
// }

// bool LifeStableState::PropagateStable() {
//   bool done = false;
//   while (!done) {
//     auto result = PropagateStableStep();
//     if (!result.first) {
//       return false;
//     }
//     done = result.second;
//   }
//   return true;
// }

std::pair<bool, bool> LifeStableState::PropagateStableStep() {
  LifeState startUnknownStable = unknownStable;

  LifeState dummy(false);
  CountNeighbourhood(state, dummy, state2, state1, state0);
  CountNeighbourhood(unknownStable, unknown3, unknown2, unknown1, unknown0);

  LifeState new_off(false), new_on(false), new_signal_off(false), new_signal_on(false);

  uint64_t has_set_off = 0;
  uint64_t has_set_on = 0;
  uint64_t has_signal_off = 0;
  uint64_t has_signal_on = 0;
  uint64_t has_abort = 0;

  for (int i = 0; i < N; i++) {
    uint64_t on2 = state2.state[i];
    uint64_t on1 = state1.state[i];
    uint64_t on0 = state0.state[i];

    uint64_t unk3 = unknown3.state[i];
    uint64_t unk2 = unknown2.state[i];
    uint64_t unk1 = unknown1.state[i];
    uint64_t unk0 = unknown0.state[i];

    unk1 |= unk2 | unk3;
    unk0 |= unk2 | unk3;

    uint64_t state0 = state.state[i];
    uint64_t state1 = unknownStable.state[i];
    uint64_t gl     = glanced.state[i];
    uint64_t dr     = glancedON.state[i];

    // These are the 5 output bits that are calculated for each cell
    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
    uint64_t signal_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

// Begin Autogenerated
set_off |= on2 ;
set_off |= (~on1) & ((~unk1) | ((~on0) & (~unk0)));
set_on |= (~on2) & on1 & on0 & (~unk1) ;
abort |= state0 & on2 & (on1 | on0) ;
abort |= state0 & (~on1) & on0 & (~unk1) ;
abort |= on1 & (~unk1) & (~unk0) & (((~state0) & (~on2) & on0) | (state0 & (~on0))) ;
signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk1) & unk0 ;
signal_off |= state0 & (~on1) & (((~on0) & unk1) | ((~unk1) & unk0));
signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk1) ;
signal_on |= state0 & on1 & (~on0) & (~unk1) ;
signal_on |= state0 & (~on1) & on0 & (~unk0) ;
// End Autogenerated

   // A glanced cell with an ON neighbour
   signal_off |= gl & (~on2) & (~on1) & on0;
   // A glanced cell with too many neighbours
   abort |= gl & (on2 | on1);
   // A glanced cell that is ON
   abort |= gl & state0;

   // A glancedON cell with 2 ON/UNK neighbours
   signal_on |= dr & (~unk3) & (~on2) & (~on1) & (~unk2) & (((~unk1) & unk0 & on0) | (unk1 & (~unk0) & (~on0)));
   // A glancedON cell with too few neighbours
   abort |= dr & (~unk3) & (~unk2) & (~unk1) & (~on2) & (~on1) & (((~unk0) & (~on0)) | (unk0 & (~on0)) | ((~unk0) & on0));
   // A glancedON cell that is ON
   abort |= dr & state0;

   new_off.state[i] = set_off & state1;
   new_on.state[i] = set_on & state1;
   new_signal_off.state[i] = signal_off;
   new_signal_on.state[i] = signal_on;

   has_set_off |= set_off;
   has_set_on |= set_on;
   has_signal_off |= signal_off;
   has_signal_on |= signal_on;
   has_abort |= abort;
  }

  if(has_abort != 0)
    return std::make_pair(false, false);

  if (has_set_on != 0) {
    state |= new_on;
    unknownStable &= ~new_on;
  }

  if (has_set_off != 0) {
    unknownStable &= ~new_off;
  }

  LifeState off_zoi(false);
  LifeState on_zoi(false);
  if (has_signal_off != 0) {
    off_zoi = new_signal_off.ZOI();
    unknownStable &= ~off_zoi;
  }

  if (has_signal_on != 0) {
    on_zoi = new_signal_on.ZOI();
    state |= on_zoi & unknownStable;
    unknownStable &= ~on_zoi;
  }

  if (has_signal_on != 0 && has_signal_off != 0) {
    if(!(on_zoi & off_zoi & unknownStable).IsEmpty()) {
      has_abort = 1;
    }
  }

  return std::make_pair(has_abort == 0, unknownStable == startUnknownStable);
}

bool LifeStableState::PropagateStable() {
  bool done = false;
  while (!done) {
    auto result = PropagateStableStep();
    if (!result.first) {
      return false;
    }
    done = result.second;
  }

  stateZOI = state.ZOI();
  return true;
}

std::pair<int, int> LifeStableState::UnknownNeighbour(std::pair<int, int> cell) const {
  // This could obviously be done faster by extracting the result
  // directly from the columns, but this is probably good enough for now
  const std::array<std::pair<int, int>, 9> directions = {std::make_pair(-1,-1), {-1,0}, {-1,1}, {0,-1}, {0, 0}, {0,1}, {1, -1}, {1, 0}, {1, 1}};
  for (auto d : directions) {
    int x = (cell.first + d.first + N) % N;
    int y = (cell.second + d.second + 64) % 64;
    if (unknownStable.Get(x, y))
      return std::make_pair(x, y);
  }
  return std::make_pair(-1, -1);
}

bool LifeStableState::TestUnknowns() {
  LifeState next = state;
  next.Step();

  LifeState changes = state ^ next;
  if (changes.IsEmpty()) {
    // We win
    return true;
  }

  // Try all the nearby changes to see if any are forced
  LifeState newPlacements = changes.ZOI() & unknownStable;
  while (!newPlacements.IsEmpty()) {
    auto newPlacement = newPlacements.FirstOn();
    newPlacements.Erase(newPlacement.first, newPlacement.second);

    LifeStableState onSearch;
    LifeStableState offSearch;
    bool onConsistent;
    bool offConsistent;

    // Try on
    {
      onSearch = *this;
      onSearch.state.Set(newPlacement.first, newPlacement.second);
      onSearch.unknownStable.Erase(newPlacement.first, newPlacement.second);
      onConsistent = onSearch.PropagateStable();
    }

    // Try off
    {
      offSearch = *this;
      offSearch.state.Erase(newPlacement.first, newPlacement.second);
      offSearch.unknownStable.Erase(newPlacement.first, newPlacement.second);
      offConsistent = offSearch.PropagateStable();
    }

    if(!onConsistent && !offConsistent) {
      return false;
    }

    if(onConsistent && !offConsistent) {
      *this = onSearch;
    }

    if (!onConsistent && offConsistent) {
      *this = offSearch;
    }

    newPlacements &= unknownStable;
  }
  return true;
}

bool LifeStableState::CompleteStableStep(unsigned &maxPop, LifeState &best) {
  // std::cout << stable.RLE() << std::endl;
  if (state.GetPop() >= maxPop) {
    return false;
  }

  bool consistent = PropagateStable();
  if (!consistent)
    return false;

  if (state.GetPop() >= maxPop) {
    return false;
  }

  bool result = TestUnknowns();
  if (!result)
    return false;

  if (state.GetPop() >= maxPop) {
    return false;
  }

  LifeState next = state;
  next.Step();

  LifeState changes = state ^ next;
  if (changes.IsEmpty()) {
    // We win
    best = state;
    maxPop = state.GetPop();
    // std::cout << maxPop << std::endl;
    // std::cout << best.RLE() << std::endl;

    return true;
  }

  // Now make a guess
  LifeState newPlacements = changes.ZOI() & unknownStable;
  if(newPlacements.IsEmpty())
    return false;

  // std::cout << "x = 0, y = 0, rule = PropagateStable" << std::endl;
  // std::cout << UnknownRLE() << std::endl;
  // std::cout << newPlacements.RLE() << std::endl;
  // std::cin.get();
  auto newPlacement = newPlacements.FirstOn();
  bool onresult = false;
  bool offresult = false;

  // Try off
  {
    LifeStableState nextState = *this;
    nextState.state.Erase(newPlacement.first, newPlacement.second);
    nextState.unknownStable.Erase(newPlacement.first, newPlacement.second);
    onresult = nextState.CompleteStableStep(maxPop, best);
  }
  // Then must be on
  {
    LifeStableState nextState = *this;
    nextState.state.Set(newPlacement.first, newPlacement.second);
    nextState.unknownStable.Erase(newPlacement.first, newPlacement.second);
    offresult = nextState.CompleteStableStep(maxPop, best);
  }

  return onresult || offresult;
}

LifeState LifeStableState::CompleteStable() {
  LifeState best;
  unsigned maxPop = std::numeric_limits<int>::max();
  LifeState searchArea = state;

  auto startTime = std::chrono::system_clock::now();

  while(!(unknownStable & ~searchArea).IsEmpty()) {
    searchArea = searchArea.ZOI();
    LifeStableState copy = *this;
    copy.unknownStable &= searchArea;
    copy.CompleteStableStep(maxPop, best);

    auto currentTime = std::chrono::system_clock::now();
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

    if (best.GetPop() > 0 || seconds > 10)
      break;
  }
  return best;
}
