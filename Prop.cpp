#include "LifeAPI.h"
#include <deque>

void inline HalfAdd(uint64_t &out0, uint64_t &out1, const uint64_t ina, const uint64_t inb) {
  out0 = ina ^ inb;
  out1 = ina & inb;
}

void inline FullAdd(uint64_t &out0, uint64_t &out1, const uint64_t ina, const uint64_t inb, const uint64_t inc) {
  uint64_t halftotal = ina ^ inb;
  out0 = halftotal ^ inc;
  uint64_t halfcarry1 = ina & inb;
  uint64_t halfcarry2 = inc & halftotal;
  out1 = halfcarry1 | halfcarry2;
}

void CountRows(LifeState &state, LifeState &bit1, LifeState &bit0) {
  for (int i = 0; i < N; i++) {
    uint64_t a = state.state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    bit1.state[i] = l ^ r ^ a;
    bit0.state[i] = ((l ^ r) & a) | (l & r);
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

  std::pair<bool, bool> PropagateStableStep();
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

  LifeState oncol0, oncol1, unkcol0, unkcol1;
  CountRows(stable, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);

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

    // Sum up the number of on/unknown cells in the 3x3 square. This
    // is done for a whole 64bit column at a time, with the 4bit
    // result stored in four separate 64bit ints.

    uint64_t on3, on2, on1, on0;
    uint64_t unk3, unk2, unk1, unk0;

    {
      uint64_t u_on1 = oncol1.state[idxU];
      uint64_t u_on0 = oncol0.state[idxU];
      uint64_t c_on1 = oncol1.state[i];
      uint64_t c_on0 = oncol0.state[i];
      uint64_t l_on1 = oncol1.state[idxB];
      uint64_t l_on0 = oncol0.state[idxB];

      uint64_t uc0, uc1, uc2, uc_carry0;
      HalfAdd(uc0, uc_carry0, u_on0, c_on0);
      FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

      uint64_t on_carry1, on_carry0;
      HalfAdd(on0, on_carry0, uc0, l_on0);
      FullAdd(on1, on_carry1, uc1, l_on1, on_carry0);
      HalfAdd(on2, on3, uc2, on_carry1);

      uint64_t u_unk1 = unkcol1.state[idxU];
      uint64_t u_unk0 = unkcol0.state[idxU];
      uint64_t c_unk1 = unkcol1.state[i];
      uint64_t c_unk0 = unkcol0.state[i];
      uint64_t l_unk1 = unkcol1.state[idxB];
      uint64_t l_unk0 = unkcol0.state[idxB];

      uint64_t ucunk0, ucunk1, ucunk2, ucunk_carry0;
      HalfAdd(ucunk0, ucunk_carry0, u_unk0, c_unk0);
      FullAdd(ucunk1, ucunk2, u_unk1, c_unk1, ucunk_carry0);

      uint64_t unk_carry1, unk_carry0;
      HalfAdd(unk0, unk_carry0, ucunk0, l_unk0);
      FullAdd(unk1, unk_carry1, ucunk1, l_unk1, unk_carry0);
      HalfAdd(unk2, unk3, ucunk2, unk_carry1);
    }

    uint64_t state0 = stable.state[i];
    uint64_t state1 = unk.state[i];

    // These are the 5 output bits that are calculated for each cell
    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t signal_off = 0; // Set all UNKNOWN neighbours of the cell to OFF
    uint64_t signal_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

// Begin Autogenerated
set_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk2) & (~unk1) & unk0 ;
set_off |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
set_off |= (~state0) & (~on1) & (~on0) & (~unk2) & unk1 & (~unk0) ;
set_off |= (~state0) & (~on1) & (~unk3) & (~unk2) & (~unk1) & unk0 ;
set_off |= (~state0) & on2 & unk2 ;
set_off |= (~state0) & on2 & unk0 ;
set_off |= (~state0) & on2 & unk1 ;
set_on |= state1 & (~on2) & on1 & on0 & (~unk2) & (~unk1) & unk0 ;
set_on |= state0 & (~on1) & on0 & (~unk2) & unk1 & (~unk0) ;
set_on |= state0 & (~on2) & (~on0) & (~unk3) & (~unk2) & (~unk1) ;
set_on |= state0 & (~on2) & (~on1) & (~on0) & (~unk2) & unk1 ;
set_on |= state0 & on2 & unk2 ;
set_on |= state0 & on2 & unk0 ;
set_on |= state0 & on2 & unk1 ;
signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk2) & (~unk1) & unk0 ;
signal_off |= state0 & on2 & unk2 ;
signal_off |= state0 & on2 & unk0 ;
signal_off |= state0 & on2 & unk1 ;
signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
signal_on |= state0 & (~on1) & on0 & (~unk2) & unk1 & (~unk0) ;
signal_on |= state0 & (~on2) & (~on0) & (~unk3) & (~unk2) & (~unk1) ;
signal_on |= state0 & (~on2) & (~on1) & (~on0) & (~unk2) & unk1 ;
abort |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0) ;
abort |= state0 & on2 & on0 ;
abort |= state0 & on2 & on1 ;
abort |= state0 & (~on2) & (~on0) & (~unk3) & (~unk2) & (~unk1) & (~unk0) ;
abort |= state0 & (~on2) & (~on1) & (~on0) & (~unk2) & unk1 & (~unk0) ;
abort |= state0 & (~on2) & (~on1) & (~unk3) & (~unk2) & (~unk1) ;
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
