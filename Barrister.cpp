#include "toml/toml.hpp"

#include "LifeAPI.h"
#include <deque>
#include <limits>

void ParseTristate(const char *rle, LifeState &state0, LifeState &state1, LifeState &state2) {
  char ch;
  int cnt, i, j;
  int x, y;
  x = 0;
  y = 0;
  cnt = 0;

  i = 0;

  while ((ch = rle[i]) != '\0') {

    if (ch >= '0' && ch <= '9') {
      cnt *= 10;
      cnt += (ch - '0');
    } else if (ch == '.') {
      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        state0.SetCell(x, y, 1);
        x++;
      }

      cnt = 0;
    } else if (ch == 'A') {

      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        state1.SetCell(x, y, 1);
        x++;
      }

      cnt = 0;
    } else if (ch == 'B') {

      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        state2.SetCell(x,y,1);
        x++;
      }

      cnt = 0;
    } else if (ch == '$') {
      if (cnt == 0)
        cnt = 1;

      if (cnt == 129)
        // TODO: error
        return;

      y += cnt;
      x = 0;
      cnt = 0;
    } else if (ch == '!') {
      break;
    } else {
      // TODO: error
      return;
    }

    i++;
  }
  state0.RecalculateMinMax();
  state1.RecalculateMinMax();
  state2.RecalculateMinMax();
}

struct SearchParams {
public:
  int minFirstActiveGen;
  int maxFirstActiveGen;
  int maxActiveWindowGens;
  int maxPreInteractionChoices;
  int maxPostInteractionChoices;
  int maxActiveCells;
  int minStableInterval;

  int maxChanges;
  int changesGracePeriod;

  LifeState activePattern;
  LifeState searchArea;

  bool stabiliseResults;

  bool debug;

  static SearchParams FromToml(toml::value &toml);
};

SearchParams SearchParams::FromToml(toml::value &toml) {
  SearchParams params;

  params.minFirstActiveGen = toml::find_or(toml, "min-first-active-gen", 0);
  params.maxFirstActiveGen = toml::find_or(toml, "max-first-active-gen", 20);
  params.maxActiveWindowGens = toml::find_or(toml, "max-active-window-gens", 20);
  params.maxPreInteractionChoices = toml::find_or(toml, "max-pre-interaction-choices", 3);
  params.maxPostInteractionChoices = toml::find_or(toml, "max-post-interaction-choices", 25);
  params.maxActiveCells = toml::find_or(toml, "max-active-cells", 15);
  params.minStableInterval = toml::find_or(toml, "min-stable-interval", 5);

  params.maxChanges = toml::find_or(toml, "max-changed-cells", 100);
  params.changesGracePeriod = toml::find_or(toml, "max-changed-cells-grace-period", 5);

  params.stabiliseResults = toml::find_or(toml, "stabilise-results", false);

  params.debug = toml::find_or(toml, "debug", false);

  LifeState state0;
  LifeState state1;
  LifeState state2;

  std::string rle = toml::find<std::string>(toml, "pattern");
  ParseTristate(rle.c_str(), state0, state1, state2);

  params.activePattern = state1;
  params.searchArea = state2;

  return params;
}

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

void inline CountRows(LifeState &state, LifeState &__restrict__ bit1, LifeState &__restrict__ bit0) {
  for (int i = 0; i < N; i++) {
    uint64_t a = state.state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    bit1.state[i] = l ^ r ^ a;
    bit0.state[i] = ((l ^ r) & a) | (l & r);
  }
}

// class CountState {
// public:
//   uint64_t state[4 * N];

//   CountState(LifeState &s);
// };

// // Turn a LifeState into a 0/1 count
// CountState::CountState(LifeState &s) : state{0} {
//   for (unsigned i = 0; i < N; i++) {
//     for (unsigned r = 0; r < 4; r++) {
//       // Get 16 bits
//       uint64_t quarter =
//           RotateRight(s.state[i] & (RotateLeft(0xFFULL, 16 * r)), 16 * r);
//       // Separate them
//       uint64_t exploded = quarter * 16;
//       state[4*i+r] = exploded;
//     }
//   }
// }

class SearchState {
public:
  LifeState state;
  // Stable background
  LifeState stable;
  // Cells that are toggled at some point
  LifeState known;
  // LifeState dontcare;
  bool hasInteracted;
  unsigned interactionStartTime;
  unsigned preInteractionChoices;
  unsigned postInteractionChoices;
  unsigned stabletime;

  SearchState()
      : hasInteracted(false), interactionStartTime(0), preInteractionChoices(0),
        postInteractionChoices(0), stabletime(0) {}

  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  std::string UnknownRLE() const;
  static SearchState ParseUnknown(const char *rle);

  std::pair<bool, bool> PropagateStableStep();
  bool PropagateStable();
  bool TestUnknowns();
  bool CompleteStable(unsigned &maxPop, LifeState &best);
  LifeState CompleteStable();

  void UncertainStep(LifeState &nextUnknown, LifeState &next);

  bool RunSearch(SearchParams &params);

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

SearchState SearchState::ParseUnknown(const char *rle) {
  LifeState state0;
  LifeState state1;
  LifeState state2;

  ParseTristate(rle, state0, state1, state2);

  SearchState result;
  result.state = state1;
  result.known = state0 | state1;

  return result;
}

std::pair<bool, bool> SearchState::PropagateStableStep() {
  LifeState startKnown = known;
  LifeState unk = ~known;

  LifeState oncol0, oncol1, unkcol0, unkcol1;
  CountRows(stable, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);

  LifeState new_off, new_on, new_signal_off, new_signal_on, new_abort;

  uint64_t has_set_off = 0;
  uint64_t has_set_on = 0;
  uint64_t has_signal_off = 0;
  uint64_t has_signal_on = 0;
  uint64_t has_abort = 0;

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
set_off |= on2 ;
set_off |= (~on1) & (~unk3) & (~unk2) & (~unk1) ;
set_off |= (~on1) & (~on0) & (~unk2) & unk1 & (~unk0) ;
set_on |= (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk2) & (~unk1) & unk0 ;
signal_off |= state0 & (~on1) & (~on0) & unk2 ;
signal_off |= state0 & (~on1) & (~on0) & unk1 ;
signal_off |= state0 & (~on1) & (~unk2) & (~unk1) & unk0 ;
signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
signal_on |= state0 & on1 & (~on0) & (~unk2) & (~unk1) ;
signal_on |= state0 & (~on1) & on0 & (~unk2) & unk1 & (~unk0) ;
abort |= state0 & on2 & on1 ;
abort |= state0 & on2 & on0 ;
abort |= (~state0) & (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0) ;
abort |= state0 & (~on2) & (~on1) & (~unk3) & (~unk2) & (~unk1) ;
abort |= state0 & (~on2) & (~on0) & (~unk2) & (~unk1) & (~unk0) ;
// End Autogenerated

   new_off.state[i] = set_off;
   new_on.state[i] = set_on;
   new_signal_off.state[i] = signal_off;
   new_signal_on.state[i] = signal_on;
   new_abort.state[i] = abort;

   has_set_off |= set_off;
   has_set_on |= set_on;
   has_signal_off |= signal_off;
   has_signal_on |= signal_on;
   has_abort |= abort;
  }

  if (has_set_off != 0) {
    known |= new_off & unk;
  }

  if (has_set_on != 0) {
    stable |= new_on & unk;
    state |= new_on & unk;
    known |= new_on & unk;
  }

  LifeState off_zoi;
  LifeState on_zoi;
  if (has_signal_off != 0) {
    off_zoi = new_signal_off.ZOI();
    known |= off_zoi;
  }

  if (has_signal_on != 0) {
    on_zoi = new_signal_on.ZOI();
    stable |= on_zoi & unk;
    state |= on_zoi & unk;
    known |= on_zoi;
  }

  if (has_signal_on != 0 && has_signal_off != 0) {
    if(!(on_zoi & off_zoi & unk).IsEmpty()) {
      has_abort = 1;
    }
  }

  return std::make_pair(has_abort == 0, startKnown == known);
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
  return true;
}

void SearchState::UncertainStep(LifeState &nextUnknown, LifeState &next) {
  LifeState result;

  LifeState unk = ~known;
  LifeState sta = ~(state ^ stable);

  LifeState oncol0, oncol1, unkcol0, unkcol1, stacol0, stacol1;
  CountRows(state, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);
  CountRows(sta, stacol0, stacol1);

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

    uint64_t on3, on2, on1, on0;
    uint64_t unk3, unk2, unk1, unk0;
    uint64_t sta3, sta2, sta1, sta0;

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

      uint64_t u_sta1 = stacol1.state[idxU];
      uint64_t u_sta0 = stacol0.state[idxU];
      uint64_t c_sta1 = stacol1.state[i];
      uint64_t c_sta0 = stacol0.state[i];
      uint64_t l_sta1 = stacol1.state[idxB];
      uint64_t l_sta0 = stacol0.state[idxB];

      uint64_t ucsta0, ucsta1, ucsta2, ucsta_carry0;
      HalfAdd(ucsta0, ucsta_carry0, u_sta0, c_sta0);
      FullAdd(ucsta1, ucsta2, u_sta1, c_sta1, ucsta_carry0);

      uint64_t sta_carry1, sta_carry0;
      HalfAdd(sta0, sta_carry0, ucsta0, l_sta0);
      FullAdd(sta1, sta_carry1, ucsta1, l_sta1, sta_carry0);
      HalfAdd(sta2, sta3, ucsta2, sta_carry1);
    }

    uint64_t stateon = state.state[i];
    uint64_t stateunk = unk.state[i];
    uint64_t statesta = sta.state[i];

    uint64_t next_on = 0;
    uint64_t unknown = 0;

// Begin Autogenerated
next_on |= stateon & statesta & sta3 & sta0 ;
next_on |= stateon & (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
next_on |= stateunk & (~statesta) & (~on2) & on1 & on0 & (~unk2) & (~unk1) ;
next_on |= (~statesta) & (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0) ;
next_on |= stateon & on2 & (~on1) & (~on0) & (~unk2) & (~unk1) & (~unk0) ;
next_on |= stateunk & (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~sta3) ;
next_on |= (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0) & (~sta3) ;
next_on |= stateunk & (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~sta0) ;
next_on |= (~on2) & on1 & on0 & (~unk2) & (~unk1) & (~unk0) & (~sta0) ;
unknown |= (~stateon) & on2 ;
unknown |= on2 & on1 ;
unknown |= on2 & on0 ;
unknown |= statesta & sta3 & sta0 ;
unknown |= (~on2) & (~on1) & (~unk3) & (~unk2) & (~unk1) ;
unknown |= (~unk3) & (~unk2) & (~unk1) & (~unk0) ;
unknown |= stateunk & statesta & (~unk3) & (~unk2) & (~unk1) ;
unknown |= stateon & on0 & (~unk3) & (~unk2) & (~unk1) ;
unknown |= stateunk & on0 & (~unk2) & (~unk1) & unk0 ;
unknown |= (~on2) & (~on1) & (~on0) & (~unk3) & (~unk2) & (~unk0) ;
// End Autogenerated

    next.state[i] = next_on;
    nextUnknown.state[i] = ~unknown; // "phase" in the espresso result is 0
  }
  next.gen = state.gen + 1;
}

// Invariants that should *always* be true
bool SearchState::CheckSanity() {
  LifeState interior = ~(~known).ZOI();

  LifeState nextStable = stable;
  nextStable.Step();

  bool stableIsStable = (stable & interior) == (nextStable & interior);

  //  bool unknownDisjoint = (~known & stable).IsEmpty() && (~known & active).IsEmpty();

  return stableIsStable;
}

bool SearchState::TestUnknowns() {
  LifeState next = stable;
  next.Step();

  LifeState changes = stable ^ next;
  if (changes.IsEmpty()) {
    // We win
    return true;
  }

  // Try all the nearby changes to see if any are forced
  LifeState newPlacements = changes.ZOI() & ~known;
  while (!newPlacements.IsEmpty()) {
    auto newPlacement = newPlacements.FirstOn();
    newPlacements.Erase(newPlacement.first, newPlacement.second);

    SearchState onSearch;
    SearchState offSearch;
    bool onConsistent;
    bool offConsistent;

    // Try on
    {
      onSearch = *this;
      onSearch.stable.Set(newPlacement.first, newPlacement.second);
      onSearch.known.Set(newPlacement.first, newPlacement.second);
      onConsistent = onSearch.PropagateStable();
    }

    // Try off
    {
      offSearch = *this;
      offSearch.stable.Erase(newPlacement.first, newPlacement.second);
      offSearch.known.Set(newPlacement.first, newPlacement.second);
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

    newPlacements &= ~known;
  }
  return true;
}

LifeState SearchState::CompleteStable() {
  SearchState copy = *this;
  LifeState best;
  unsigned maxPop = std::numeric_limits<int>::max();
  copy.CompleteStable(maxPop, best);
  return best;
}

bool SearchState::CompleteStable(unsigned &maxPop, LifeState &best) {
  if (stable.GetPop() >= maxPop) {
    return false;
  }

  bool consistent = PropagateStable();
  if (!consistent)
    return false;

  if (stable.GetPop() >= maxPop) {
    return false;
  }

  bool result = TestUnknowns();
  if (!result)
    return false;

  if (stable.GetPop() >= maxPop) {
    return false;
  }

  LifeState next = stable;
  next.Step();

  LifeState changes = stable ^ next;
  if (changes.IsEmpty()) {
    // We win
    best = stable;
    maxPop = stable.GetPop();
    // std::cout << maxPop << std::endl;
    // std::cout << best.RLE() << std::endl;

    return true;
  }


  // Now make a guess
  LifeState newPlacements = changes.ZOI() & ~known;
  // std::cout << "x = 0, y = 0, rule = PropagateStable" << std::endl;
  // std::cout << UnknownRLE() << std::endl;
  // std::cout << newPlacements.RLE() << std::endl;
  // std::cin.get();
  auto newPlacement = newPlacements.FirstOn();
  // Try off
  {
    SearchState nextState = *this;
    nextState.stable.Erase(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    bool result = nextState.CompleteStable(maxPop, best);
  }
  // Then must be on
  {
    SearchState nextState = *this;
    nextState.stable.Set(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    bool result = nextState.CompleteStable(maxPop, best);
  }
  return false;
}

bool SearchState::RunSearch(SearchParams &params) {
  bool debug = params.debug;

  if (!hasInteracted && state.gen > params.maxFirstActiveGen) {
    if(debug) std::cout << "failed: didn't interact before " << params.maxFirstActiveGen << std::endl;
    return false;
  }
  if (hasInteracted && state.gen - interactionStartTime > params.maxActiveWindowGens) {
    if (debug) std::cout << "failed: too long " << stable.RLE() << std::endl;

    return false;
  }

  if (preInteractionChoices > params.maxPreInteractionChoices) {
    if (debug) std::cout << "failed: too many pre-interaction choices " << stable.RLE() << std::endl;
    return false;
  }

  if (postInteractionChoices > params.maxPostInteractionChoices) {
    if (debug) std::cout << "failed: too many post-interaction choices " << stable.RLE() << std::endl;
    return false;
  }

  if (stabletime > params.minStableInterval) {
    std::cout << "Success:" << std::endl;
    std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
    std::cout << UnknownRLE() << std::endl << std::flush;
    if (params.stabiliseResults) {
      std::cout << "Stabilising:" << std::endl;
      LifeState completed = CompleteStable();
      std::cout << completed.RLE() << std::endl << std::flush;
    }
    // std::cout << stable.RLE() << std::endl << std::flush;
    // std::cout << preInteractionChoices << std::endl << std::flush;
    // std::cout << postInteractionChoices << std::endl << std::flush;
    // std::cout << "minimising:" << std::endl;
    // LifeState completed = CompleteStable();
    // std::cout << completed.RLE() << std::endl << std::flush;
    // We win
    return true;
  }

  bool consistent = PropagateStable();
  bool consistent2 = true;
  // bool consistent2 = TestUnknowns();

  if (!consistent || !consistent2) {
    if (debug) std::cout << "failed: inconsistent" << std::endl;
    return false;
  }

  LifeState nextUnknowns;
  LifeState next;
  UncertainStep(nextUnknowns, next);
  LifeState nearbyUnknowns = ~known & nextUnknowns.ZOI();

  if (nearbyUnknowns.IsEmpty()) {
    if (!hasInteracted) {
      // See if there is any difference caused by the stable cells
      LifeState stateonly = state & ~stable;
      stateonly.Step();
      if (stateonly != (next & ~stable)) {
        if (state.gen < params.minFirstActiveGen) return false;
        hasInteracted = true;
        interactionStartTime = state.gen;
        // std::cout << "x = 0, y = 0, rule = PropagateStable" << std::endl;
        // std::cout << UnknownRLE() << std::endl << std::flush;
      }
    }
    LifeState stableZOI = stable.ZOI();

    LifeState changes = (state ^ next) & stableZOI;
    if (state.gen - interactionStartTime > params.changesGracePeriod && changes.GetPop() > params.maxChanges) {
      if (debug) std::cout << "failed: too many changes " << stable.RLE() << std::endl;
      return false;
    }

    // We can safely take a step
    state = next;

    LifeState actives = (stable ^ state) & stableZOI;
    if (hasInteracted && actives.IsEmpty()) {
      stabletime += 1;
    } else {
      stabletime = 0;
    }

    if (actives.GetPop() > params.maxActiveCells) {
      if (debug) std::cout << "failed: too many active " << stable.RLE() << std::endl;
      return false;
    }

    return RunSearch(params);
  } else {
    // Set an unknown cell and recur

    // Prefer setting an orthogonal cell to a diagonal cell
    LifeState mooreUnknowns = ~known & nextUnknowns.MooreZOI();
    std::pair<int, int> unknown;
    if (!mooreUnknowns.IsEmpty()) {
      unknown = mooreUnknowns.FirstOn();
    } else {
      unknown = nearbyUnknowns.FirstOn();
    }

    bool whichFirst = !hasInteracted;
    {
      SearchState nextState = *this;

      if(!hasInteracted && whichFirst)
        nextState.preInteractionChoices += 1;
      if(hasInteracted)
        nextState.postInteractionChoices += 1;

      nextState.state.SetCell(unknown.first, unknown.second, whichFirst);
      nextState.stable.SetCell(unknown.first, unknown.second, whichFirst);
      nextState.known.Set(unknown.first, unknown.second);
      bool result = nextState.RunSearch(params);
      // if (result) {
      //   *this = nextState;
      //   return true;
      // }
    }
    {
      SearchState nextState = *this;

      if(!hasInteracted && !whichFirst)
        nextState.preInteractionChoices += 1;
      if(hasInteracted)
        nextState.postInteractionChoices += 1;

      nextState.state.SetCell(unknown.first, unknown.second, !whichFirst);
      nextState.stable.SetCell(unknown.first, unknown.second, !whichFirst);
      nextState.known.Set(unknown.first, unknown.second);
      bool result = nextState.RunSearch(params);
      // if (result) {
      //   *this = nextState;
      //   return true;
      // }
    }

    return false;
  }

}

int main(int argc, char *argv[]) {
  auto toml = toml::parse(argv[1]);
  SearchParams params = SearchParams::FromToml(toml);

  SearchState search;
  search.state = params.activePattern;
  search.known = ~params.searchArea;

  bool result = search.RunSearch(params);
  if (result) {
    search.stable.Print();
    exit(0);
  }
}
