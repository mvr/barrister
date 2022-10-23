#include "LifeAPI.h"
#include <deque>

void CountRows(LifeState &state, LifeState &bit1, LifeState &bit0) {
  for (int i = 0; i < N; i++) {
    uint64_t a = state.state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    bit1.state[i] = l ^ r ^ a;
    bit0.state[i] = ((l ^ r) & a) | (l & r);
  }
}

void CountNeighbours(LifeState &state, LifeState &zero, LifeState &one, LifeState &two, LifeState &three, LifeState &four, LifeState &more) {

  uint64_t triplezero[N];
  uint64_t tripleone[N];
  uint64_t tripletwo[N];
  uint64_t triplethree[N];

  for (int i = 0; i < N; i++) {
    uint64_t a = state.state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);
    triplezero[i] = ~l & ~a & ~r;
    tripleone[i] = l ^ a ^ r ^ (l & a & r);
    tripletwo[i] = l ^ a ^ r ^ (l | a | r);
    triplethree[i] = l & a & r;
  }

  for (int i = 0; i < N; i++) {
    int idxU;
    int idxB;
    if (i == 0)
      idxU = N - 1;
    else
      idxU = i - 1;

    if (i == N - 1)
      idxB = 0;
    else
      idxB = i + 1;

    zero.state[i] = (triplezero[idxU] & triplezero[i] & triplezero[idxB]);
    one.state[i] = (tripleone[idxU] & triplezero[i] & triplezero[idxB]) |
                       (triplezero[idxU] & tripleone[i] & triplezero[idxB]) |
                       (triplezero[idxU] & triplezero[i] & tripleone[idxB]);
    two.state[i] = (tripletwo[idxU] & triplezero[i] & triplezero[idxB]) |
                   (triplezero[idxU] & tripletwo[i] & triplezero[idxB]) |
                   (triplezero[idxU] & triplezero[i] & tripletwo[idxB]) |
                   (triplezero[idxU] & tripleone[i] & tripleone[idxB]) |
                   (tripleone[idxU] & triplezero[i] & tripleone[idxB]) |
                   (tripleone[idxU] & tripleone[i] & triplezero[idxB]);
    three.state[i] = (triplethree[idxU] & triplezero[i] & triplezero[idxB]) |
                     (triplezero[idxU] & triplethree[i] & triplezero[idxB]) |
                     (triplezero[idxU] & triplezero[i] & triplethree[idxB]) |
                     (triplezero[idxU] & tripleone[i] & tripletwo[idxB]) |
                     (tripleone[idxU] & triplezero[i] & tripletwo[idxB]) |
                     (tripleone[idxU] & tripletwo[i] & triplezero[idxB]) |
                     (triplezero[idxU] & tripletwo[i] & tripleone[idxB]) |
                     (tripletwo[idxU] & triplezero[i] & tripleone[idxB]) |
                     (tripletwo[idxU] & tripleone[i] & triplezero[idxB]) |
                     (tripleone[idxU] & tripleone[i] & tripleone[idxB]);
    four.state[i] = (triplezero[idxU] & tripleone[i] & triplethree[idxB]) |
                    (tripleone[idxU] & triplezero[i] & triplethree[idxB]) |
                    (tripleone[idxU] & triplethree[i] & triplezero[idxB]) |
                    (triplezero[idxU] & triplethree[i] & tripleone[idxB]) |
                    (triplethree[idxU] & triplezero[i] & tripleone[idxB]) |
                    (triplethree[idxU] & tripleone[i] & triplezero[idxB]) |
                    (triplezero[idxU] & tripletwo[i] & tripletwo[idxB]) |
                    (tripletwo[idxU] & triplezero[i] & tripletwo[idxB]) |
                    (tripletwo[idxU] & tripletwo[i] & triplezero[idxB]) |
                    (tripletwo[idxU] & tripleone[i] & tripleone[idxB]) |
                    (tripleone[idxU] & tripletwo[i] & tripleone[idxB]) |
                    (tripleone[idxU] & tripleone[i] & tripletwo[idxB]);
    more.state[i] = ~(zero.state[i] | one.state[i] | two.state[i] | three.state[i] | four.state[i]);
  }
}

class CountState {
public:
  uint64_t state[4 * N];

  CountState(LifeState &s);
};

// Turn a LifeState into a 0/1 count
CountState::CountState(LifeState &s) : state{0} {
  for (unsigned i = 0; i < N; i++) {
    for (unsigned r = 0; r < 4; r++) {
      // Get 16 bits
      uint64_t quarter =
          RotateRight(s.state[i] & (RotateLeft(0xFFULL, 16 * r)), 16 * r);
      // Separate them
      uint64_t exploded = quarter * 16;
      state[4*i+r] = exploded;
    }
  }
}

class SearchState {
public:
  LifeState state;
  // Stable background
  LifeState stable;
  LifeState known;
  LifeState dontcare;
  // Cells that are toggled at least once
  LifeState active;

  std::string UnknownRLE() const;

  static void CountNeighbours(LifeState &state, LifeState &zero, LifeState &one,
                              LifeState &two, LifeState &three, LifeState &four,
                              LifeState &more);

  std::pair<bool, bool> PropagateStableStep();
  std::pair<bool, bool> PropagateStableStep2();
  bool PropagateStable();



  bool CheckSanity();
};

std::string SearchState::UnknownRLE() const {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < N; j++) {
    unsigned last_val = (stable.GetCell(0 - 32, j - 32) == 1) + ((known.GetCell(0 - 32, j - 32) == 0) << 1);
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      unsigned val = (stable.GetCell(i - 32, j - 32) == 1) + ((known.GetCell(i - 32, j - 32) == 0) << 1);

      // Flush linefeeds if we find a live cell
      if (val && eol_count > 0) {
        if (eol_count > 1)
          result << eol_count;

        result << "$";

        eol_count = 0;
      }

      // Flush current run if val changes
      if (val != last_val) {
        if (run_count > 1)
          result << run_count;

        switch(last_val) {
        case 0: result << "."; break;
        case 1: result << "A"; break;
        case 2: result << "B"; break;
        }

        run_count = 0;
      }

      run_count++;
      last_val = val;
    }

    // Flush run of live cells at end of line
    if (last_val) {
      if (run_count > 1)
        result << run_count;

      switch(last_val) {
      case 0: result << "."; break;
      case 1: result << "A"; break;
      case 2: result << "B"; break;
      }

      run_count = 0;
    }

    eol_count++;
  }

  // Flush trailing linefeeds
  if (eol_count > 0) {
    if (eol_count > 1)
      result << eol_count;

    result << "$";

    eol_count = 0;
  }

  return result.str();
}

std::pair<bool, bool> SearchState::PropagateStableStep() {
  LifeState startKnown = known;
  LifeState unk = ~known;

  LifeState on0, on1, unk0, unk1;
  CountRows(stable, on0, on1);
  CountRows(unk, unk0, unk1);

  LifeState new_off, new_on, new_signal_off, new_signal_on, new_abort;

  for (int i = 0; i < N; i++) {
    int idxU;
    int idxB;
    if (i == 0)
      idxU = N - 1;
    else
      idxU = i - 1;

    if (i == N - 1)
      idxB = 0;
    else
      idxB = i + 1;

    uint64_t u_on1 = on1.state[idxU];
    uint64_t u_on0 = on0.state[idxU];
    uint64_t c_on1 = on1.state[i];
    uint64_t c_on0 = on0.state[i];
    uint64_t l_on1 = on1.state[idxB];
    uint64_t l_on0 = on0.state[idxB];

    uint64_t u_unk1 = unk1.state[idxU];
    uint64_t u_unk0 = unk0.state[idxU];
    uint64_t c_unk1 = unk1.state[i];
    uint64_t c_unk0 = unk0.state[i];
    uint64_t l_unk1 = unk1.state[idxB];
    uint64_t l_unk0 = unk0.state[idxB];

    uint64_t state0 = stable.state[i];
    uint64_t state1 = unk.state[i];

    uint64_t set_on = 0;
    uint64_t set_off = 0;
    uint64_t signal_off = 0;
    uint64_t signal_on = 0;
    uint64_t abort = 0;

// Begin Autogenerated
signal_off |= (~state1) & (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state1) & (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state1) & (~state0) & (~u_on1) & (~u_on0) & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state1) & u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state1) & (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state1) & (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & c_unk0 & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~state0) & u_on1 & (~u_on0) & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~state0) & (~u_on1) & (~u_on0) & c_on0 & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~state0) & (~u_on1) & u_on0 & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
abort |= (~state0) & u_on1 & (~u_on0) & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= (~state0) & (~u_on1) & (~u_on0) & c_on0 & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
signal_on |= (~state1) & u_on1 & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state1) & (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state0) & (~u_on1) & (~u_on0) & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
signal_off |= (~state0) & (~u_on1) & (~u_on0) & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
abort |= (~state0) & (~u_on1) & u_on0 & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
signal_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
abort |= u_on1 & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & u_unk0 & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_off |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & l_unk0 ;
abort |= state0 & (~u_on1) & (~u_on0) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & u_on1 & (~u_on0) & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & (~u_on0) & c_on0 & l_on1 & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & u_on1 & (~u_on0) & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= (~state0) & (~u_on1) & (~u_on0) & c_on0 & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
set_on |= u_on1 & (~u_on0) & (~c_on1) & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & (~u_on0) & (~c_on1) & c_on0 & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & (~u_on0) & c_on1 & c_on0 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & u_on0 & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & l_on0 & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & u_on0 & c_on1 & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= (~state0) & (~u_on1) & (~u_on0) & c_on1 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
set_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
set_on |= u_on1 & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~state0) & (~u_on1) & u_on0 & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
set_on |= (~u_on1) & u_on0 & c_on1 & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & (~u_on0) & c_on1 & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_on |= (~u_on1) & u_on0 & (~c_on1) & c_on0 & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= u_on1 & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & l_on0 & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= u_on1 & u_on0 & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= (~u_on1) & u_on0 & (~c_on1) & (~c_on0) & l_on1 & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= u_on1 & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & l_on1 & l_on0 & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk0) ;
abort |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
abort |= state0 & (~u_on1) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
abort |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) ;
set_off |= (~u_on1) & (~c_on1) & (~c_on0) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
set_off |= (~u_on1) & (~u_on0) & (~c_on1) & (~c_on0) & (~l_on1) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~u_unk1) & (~c_unk1) & (~c_unk0) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~c_unk0) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) ;
signal_on |= state0 & (~u_on1) & (~c_on1) & (~l_on1) & (~l_on0) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_on |= state0 & (~u_on1) & (~u_on0) & (~c_on1) & (~l_on1) & (~u_unk1) & (~u_unk0) & (~c_unk1) & (~l_unk1) & (~l_unk0) ;
signal_off |= c_on1 & c_on0 & l_on0 & (~c_unk1) & (~c_unk0) & l_unk1 ;
signal_off |= u_on0 & c_on1 & c_on0 & (~c_unk1) & (~c_unk0) & l_unk1 ;
signal_off |= c_on1 & c_on0 & l_on0 & (~c_unk1) & (~c_unk0) & l_unk0 ;
signal_off |= u_on0 & c_on1 & c_on0 & (~c_unk1) & (~c_unk0) & l_unk0 ;
signal_off |= state0 & c_on1 & c_on0 & l_on0 & c_unk1 ;
signal_off |= state0 & u_on0 & c_on1 & c_on0 & c_unk1 ;
signal_off |= state0 & c_on1 & c_on0 & l_on0 & u_unk1 ;
signal_off |= state0 & u_on0 & c_on1 & c_on0 & u_unk1 ;
abort |= state0 & u_on0 & c_on1 & c_on0 & l_on0 ;
signal_off |= state0 & c_on1 & c_on0 & l_on0 & c_unk0 ;
signal_off |= state0 & u_on0 & c_on1 & c_on0 & c_unk0 ;
signal_off |= state0 & c_on1 & c_on0 & l_on0 & u_unk0 ;
signal_off |= state0 & u_on0 & c_on1 & c_on0 & u_unk0 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & l_unk1 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & c_unk1 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & u_unk1 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & l_unk0 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & c_unk0 ;
signal_off |= state0 & u_on0 & (~c_on0) & l_on0 & u_unk0 ;
abort |= state0 & c_on1 & c_on0 & l_on1 ;
abort |= state0 & u_on1 & c_on1 & c_on0 ;
abort |= state0 & u_on0 & c_on1 & l_on1 ;
abort |= state0 & u_on1 & u_on0 & c_on1 ;
signal_off |= state0 & (~c_on0) & l_on1 & l_unk1 ;
signal_off |= state0 & u_on1 & (~c_on0) & l_unk1 ;
signal_off |= state0 & (~c_on0) & l_on1 & c_unk1 ;
signal_off |= state0 & u_on1 & (~c_on0) & c_unk1 ;
signal_off |= state0 & (~c_on0) & l_on1 & u_unk1 ;
signal_off |= state0 & u_on1 & (~c_on0) & u_unk1 ;
signal_off |= state0 & l_on1 & l_on0 & l_unk1 ;
signal_off |= state0 & u_on1 & l_on0 & l_unk1 ;
signal_off |= state0 & u_on0 & l_on1 & l_unk1 ;
signal_off |= state0 & u_on1 & u_on0 & l_unk1 ;
signal_off |= state0 & l_on1 & l_on0 & c_unk1 ;
signal_off |= state0 & u_on1 & l_on0 & c_unk1 ;
signal_off |= state0 & u_on0 & l_on1 & c_unk1 ;
signal_off |= state0 & u_on1 & u_on0 & c_unk1 ;
signal_off |= state0 & l_on1 & l_on0 & u_unk1 ;
signal_off |= state0 & u_on1 & l_on0 & u_unk1 ;
signal_off |= state0 & u_on0 & l_on1 & u_unk1 ;
signal_off |= state0 & u_on1 & u_on0 & u_unk1 ;
abort |= state0 & (~c_on0) & l_on1 & l_on0 ;
abort |= state0 & u_on1 & (~c_on0) & l_on0 ;
signal_off |= state0 & (~c_on0) & l_on1 & l_unk0 ;
signal_off |= state0 & u_on1 & (~c_on0) & l_unk0 ;
signal_off |= state0 & (~c_on0) & l_on1 & c_unk0 ;
signal_off |= state0 & u_on1 & (~c_on0) & c_unk0 ;
signal_off |= state0 & (~c_on0) & l_on1 & u_unk0 ;
signal_off |= state0 & u_on1 & (~c_on0) & u_unk0 ;
abort |= state0 & u_on0 & l_on1 & l_on0 ;
abort |= state0 & u_on1 & u_on0 & l_on0 ;
signal_off |= state0 & l_on1 & l_on0 & l_unk0 ;
signal_off |= state0 & u_on1 & l_on0 & l_unk0 ;
signal_off |= state0 & u_on0 & l_on1 & l_unk0 ;
signal_off |= state0 & u_on1 & u_on0 & l_unk0 ;
signal_off |= state0 & l_on1 & l_on0 & c_unk0 ;
signal_off |= state0 & u_on1 & l_on0 & c_unk0 ;
signal_off |= state0 & u_on0 & l_on1 & c_unk0 ;
signal_off |= state0 & u_on1 & u_on0 & c_unk0 ;
signal_off |= state0 & l_on1 & l_on0 & u_unk0 ;
signal_off |= state0 & u_on1 & l_on0 & u_unk0 ;
signal_off |= state0 & u_on0 & l_on1 & u_unk0 ;
signal_off |= state0 & u_on1 & u_on0 & u_unk0 ;
set_off |= c_on1 & c_on0 & l_on0 ;
set_off |= u_on0 & c_on1 & c_on0 ;
set_off |= c_on0 & l_on1 & l_on0 ;
set_off |= u_on1 & c_on0 & l_on0 ;
set_off |= u_on0 & c_on0 & l_on1 ;
set_off |= u_on1 & u_on0 & c_on0 ;
set_off |= u_on0 & c_on1 & l_on0 ;
set_off |= u_on0 & l_on1 & l_on0 ;
set_off |= u_on1 & u_on0 & l_on0 ;
abort |= state0 & u_on1 & l_on1 ;
set_off |= c_on1 & l_on1 ;
set_off |= u_on1 & c_on1 ;
set_off |= u_on1 & l_on1 ;
// End Autogenerated

   new_off.state[i] = set_off;
   new_on.state[i] = set_on;
   new_signal_off.state[i] = signal_off;
   new_signal_on.state[i] = signal_on;
   new_abort.state[i] = abort;
  }

  stable |= (new_on | new_signal_on.ZOI()) & unk;
  known |= new_signal_on.ZOI() | new_signal_off.ZOI() | ((new_on | new_off) & unk);

  bool consistent = new_abort.IsEmpty();
  return std::make_pair(consistent, startKnown == known);
}

std::pair<bool, bool> SearchState::PropagateStableStep2() {
  LifeState zero, one, two, three, four, more;
  LifeState unkzero, unkone, unktwo, unkthree, unkfour, unkmore;
  LifeState actzero, actone, acttwo, actthree, actfour, actmore;
  LifeState startKnown = known;
  LifeState unk = ~known;

  // Remember these include the center cell
  CountNeighbours(stable, zero, one, two, three, four, more);
  CountNeighbours(unk, unkzero, unkone, unktwo, unkthree, unkfour, unkmore);
  CountNeighbours(active, actzero, actone, acttwo, actthree, actfour, actmore);

  LifeState existingON = stable & ~unk;
  LifeState existingOFF = ~stable & ~unk;

  LifeState forceON = three & unkone & unk;
  stable |= forceON;
  known |= forceON;

  LifeState forceOFF = more | (zero & unk & unkone) | (one & unk & unkone) | (zero & unk & unktwo);
  known |= forceOFF;

  LifeState inconsistentON = existingON & (more | (one & unkzero) | (two & unkzero) | (one & unkone));
  LifeState inconsistentOFF = existingOFF & (three & unkzero);

  // Unk neighbours have to be ON
  LifeState needsON = (existingON & ((one & unktwo) | (two & unkone))) |
                      (existingOFF & three & unkone);
  stable |= needsON.ZOI() & unk;
  known |= needsON.ZOI();

  LifeState needsOFF = (existingON & four) | (existingOFF & two & unkone);
  known |= needsOFF.ZOI();

  return std::make_pair((inconsistentON | inconsistentOFF).IsEmpty(), startKnown == known);
}

bool SearchState::PropagateStable() {
  bool done = false;
  while (!done) {
    auto result = PropagateStableStep();
    if (!result.first) {
      return false;
    }
    done = result.second;
  }
  state |= stable;
  return true;
}

// Invariants that should *always* be true
bool SearchState::CheckSanity() {
  LifeState interior = ~(~known).ZOI();

  LifeState nextStable = stable;
  nextStable.Step();

  bool stableIsStable = (stable & interior) == (nextStable & interior);

  bool unknownDisjoint = (~known & stable).IsEmpty() && (~known & active).IsEmpty();

  return stableIsStable && unknownDisjoint;
}

void StepWKnown(LifeState &state, LifeState &known) {
  LifeState zero, one, two, three, four, more;
  LifeState unkzero, unkone, unktwo, unkthree, unkfour, unkmore;
  LifeState unk = ~known;

  CountNeighbours(state, zero, one, two, three, four, more);
  CountNeighbours(unk, unkzero, unkone, unktwo, unkthree, unkfour, unkmore);

  LifeState OFFstable = (four | more) | (zero & (unkzero | unkone | unktwo)) | (one & (unkzero | unkone));
  LifeState OFFbirth = three & unkzero;

  LifeState ONstable = (three & (unkzero | unkone)) | (four & unkzero);
  LifeState ONdeath = more | (one & (unkzero | unkone)) | (two & unkzero);

  state = (~state & OFFbirth) | (state & ONstable);
  known &= (~state & OFFstable) | (~state & OFFbirth) | (state & ONstable) | (state & ONdeath);
}

void Run(SearchState state) {
  bool consistent = state.PropagateStable();
  if (!consistent)
    return;

  std::cout << "x = 0, y = 0, rule = PropagateStable" << std::endl;
  std::cout << state.UnknownRLE() << std::endl;

  LifeState next = state.stable;
  next.Step();

  LifeState changes = state.stable ^ next;
  if (changes.IsEmpty()) {
    // We win
    state.stable.Print();
    exit(0);
  }
  // LifeState interior = ~(~state.known).ZOI();

  // if (!(changes & interior).IsEmpty()) {
  //   // We lose
  //   return;
  // }

  // Try all the nearby changes to see if any are forced
  LifeState newPlacements = changes.ZOI() & ~state.known;
  while (!newPlacements.IsEmpty()) {
    auto newPlacement = newPlacements.FirstOn();
    newPlacements.Erase(newPlacement.first, newPlacement.second);

    bool offConsistent;
    bool onConsistent;

    // Try on
    {
      SearchState nextState = state;
      nextState.stable.Set(newPlacement.first, newPlacement.second);
      nextState.known.Set(newPlacement.first, newPlacement.second);
      onConsistent = nextState.PropagateStable();
    }

    // Try off
    {
      SearchState nextState = state;
      nextState.stable.Erase(newPlacement.first, newPlacement.second);
      nextState.known.Set(newPlacement.first, newPlacement.second);
      offConsistent = nextState.PropagateStable();
    }

    if(!onConsistent && !offConsistent) {
      return;
    }

    if(onConsistent && !offConsistent) {
      state.stable.Set(newPlacement.first, newPlacement.second);
      state.known.Set(newPlacement.first, newPlacement.second);
    }

    if(!onConsistent && offConsistent) {
      state.stable.Erase(newPlacement.first, newPlacement.second);
      state.known.Set(newPlacement.first, newPlacement.second);
    }
  }

  // Now make a guess
  newPlacements = changes.ZOI() & ~state.known;
  auto newPlacement = newPlacements.FirstOn();
  // Try off
  {
    SearchState nextState = state;
    nextState.stable.Set(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    Run(nextState);
  }
  // Then must be on
  {
    SearchState nextState = state;
    nextState.stable.Erase(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    Run(nextState);
  }
}

int main(int argc, char *argv[]) {
  LifeState start = LifeState::Parse(argv[1], 0, 0);

  SearchState state;
  state.stable = start;
  state.state = start;
  state.known = start;

  LifeState searchArea = start;
  // Run(state, fixedMask, std::deque<std::pair<int, int>>());
  while (true) {
    searchArea = searchArea.ZOI();
    state.known = start | ~searchArea;
    Run(state);
  }
}

// Idea: try inactive off, then inactive on, then active off, then active on
// Active cells should always be next to other active cells
// Maybe try active on first?
//
// After choosing a cell, need to check that it can be locally completed to a
// still life
// Leave all the known cells, add a sea of dontcare a couple of cells away
//
// Active should only be set when it actually changes in a generation.

// Is StepWUnknown even what we want?  We need to do something like
// what Bellman does: stable non-active neighbourhoods that only have
// unknown neighbours should stay stable

// Need to settle on a collection of bits for each cell, probably different ways
// to factor it.

// 1.

// Also remember, the tempand and tempxor variables are basically
// giving 2-bit counts for the number of neighbours, so we could use
// the same sort of espresso trick that Bellman uses
