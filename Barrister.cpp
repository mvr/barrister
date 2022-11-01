#include "toml/toml.hpp"

#include "LifeAPI.h"
#include <deque>
#include <limits>
#include <chrono>

void ParseTristate(const char *rle, LifeState &stateon, LifeState &statemarked) {
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
      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        switch(ch) {
        case 'A':
          stateon.Set(x, y);
          break;
        case 'B':
          statemarked.Set(x, y);
          break;
        case 'C':
          stateon.Set(x, y);
          statemarked.Set(x, y);
          break;
        }
        x++;
      }

      cnt = 0;
    }

    i++;
  }
  stateon.RecalculateMinMax();
  statemarked.RecalculateMinMax();
}

struct SearchParams {
public:
  int minFirstActiveGen;
  int maxFirstActiveGen;
  int minActiveWindowGens;
  int maxActiveWindowGens;
  int maxPreInteractionChoices;
  int maxPostInteractionChoices;
  int maxActiveCells;
  int minStableInterval;

  int maxChanges;
  int changesGracePeriod;

  LifeState activePattern;
  LifeState startingStable;
  LifeState searchArea;

  bool stabiliseResults;

  bool debug;

  static SearchParams FromToml(toml::value &toml);
};

SearchParams SearchParams::FromToml(toml::value &toml) {
  SearchParams params;

  params.minFirstActiveGen = toml::find_or(toml, "min-first-active-gen", 0);
  params.maxFirstActiveGen = toml::find_or(toml, "max-first-active-gen", 20);
  params.minActiveWindowGens = toml::find_or(toml, "min-active-window-gens", 0);
  params.maxActiveWindowGens = toml::find_or(toml, "max-active-window-gens", 20);
  params.maxPreInteractionChoices = toml::find_or(toml, "max-pre-interaction-choices", 3);
  params.maxPostInteractionChoices = toml::find_or(toml, "max-post-interaction-choices", 25);
  params.maxActiveCells = toml::find_or(toml, "max-active-cells", 15);
  params.minStableInterval = toml::find_or(toml, "min-stable-interval", 5);

  params.maxChanges = toml::find_or(toml, "max-changed-cells", 100);
  params.changesGracePeriod = toml::find_or(toml, "max-changed-cells-grace-period", 5);

  params.stabiliseResults = toml::find_or(toml, "stabilise-results", false);

  params.debug = toml::find_or(toml, "debug", false);

  LifeState stateon;
  LifeState statemarked;

  std::string rle = toml::find<std::string>(toml, "pattern");
  ParseTristate(rle.c_str(), stateon, statemarked);

  params.activePattern = stateon & ~statemarked;
  params.startingStable = stateon & statemarked;
  params.searchArea = ~stateon & statemarked;

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

enum FocusType {
  NONE,
  NORMAL,
  GLANCING
};

struct Focus {
  FocusType type;
  std::pair<int, int> coords;

  Focus(FocusType t, std::pair<int, int> c) : type(t), coords(c) {}
  static Focus None()
  {
    return Focus(NONE, std::make_pair(-1,-1));
  }
};

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

  std::pair<int, int> UnknownNeighbour(std::pair<int, int> cell);

  bool SimplePropagateColumnStep(int column);

  std::pair<bool, bool> SimplePropagateStableStep();
  bool SimplePropagateStable();
  std::pair<bool, bool> PropagateStableStep();
  bool PropagateStable();
  bool TestUnknowns();
  bool CompleteStable(unsigned &maxPop, LifeState &best);
  LifeState CompleteStable();

  void UncertainActiveStep(LifeState &next, LifeState &nextUnknown);
  void UncertainStep(LifeState &next, LifeState &nextUnknown, LifeState &glancing);

  bool RunSearch(SearchParams &params, Focus focus, LifeState &triedGlancing);
  bool RunSearch(SearchParams &params) {
    LifeState blank;
    return RunSearch(params, Focus::None(), blank);
  }

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
  LifeState stateon;
  LifeState statemarked;

  ParseTristate(rle, stateon, statemarked);

  SearchState result;
  result.state = stateon;
  result.known = stateon | (~stateon & ~statemarked);

  return result;
}

std::pair<int, int> SearchState::UnknownNeighbour(std::pair<int, int> cell) {
  const std::vector<std::pair<int, int>> directions = {{-1,0}, {0,-1}, {1,0}, {0,1}, {-1,-1}, {-1, 1}, {1, -1}, {1, 1}};
  for (auto d : directions) {
    int x = cell.first + d.first;
    int y = cell.second + d.second;
    if (!known.GetCell(x, y))
      return std::make_pair(x, y);
  }
  return std::make_pair(-1, -1);
}

bool SearchState::SimplePropagateColumnStep(int column) {
  std::array<uint64_t, 5> nearbyStable;
  std::array<uint64_t, 5> nearbyUnknown;
  for (int i = 0; i < 5; i++) {
    int c = column + i - 2;
    if (c == -2)
      c = N-2;
    if (c == -1)
      c = N-1;
    nearbyStable[i] = stable.state[c];
    nearbyUnknown[i] = ~known.state[c];
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

// Begin Autogenerated
set_off |= on2 ;
set_off |= (~on1) & (~unk1) ;
set_off |= (~on1) & (~on0) & (~unk0) ;
set_on |= (~on2) & on1 & on0 & (~unk1) ;
abort |= state0 & on2 & on1 ;
abort |= state0 & on2 & on0 ;
abort |= (~state0) & (~on2) & on1 & on0 & (~unk1) & (~unk0) ;
abort |= state0 & (~on1) & on0 & (~unk1) ;
abort |= state0 & on1 & (~on0) & (~unk1) & (~unk0) ;
// End Autogenerated

   new_off[i] = set_off;
   new_on[i] = set_on;

   has_abort |= abort;
  }

  if(has_abort != 0)
    return false;

  for (int i = 1; i < 4; i++) {
    int orig = column + i - 2;
    if(orig < 0) orig += N;
    if(orig > N) orig -= N;
    known.state[orig] |= new_off[i] & nearbyUnknown[i];
    known.state[orig] |= new_on[i] & nearbyUnknown[i];
    stable.state[orig] |= new_on[i] & nearbyUnknown[i];
    state.state[orig] |= new_on[i] & nearbyUnknown[i];
  }

  return true;
}

std::pair<bool, bool> SearchState::SimplePropagateStableStep() {
  LifeState startKnown = known;
  LifeState unk = ~known;

  LifeState oncol0, oncol1, unkcol0, unkcol1;
  CountRows(stable, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);

  LifeState new_off, new_on;

  uint64_t has_set_off = 0;
  uint64_t has_set_on = 0;
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
      on2 |= on3;
      on1 |= on3;
      on0 |= on3;

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
      unk1 |= unk2 | unk3;
      unk0 |= unk2 | unk3;
    }

    uint64_t state0 = stable.state[i];
    uint64_t state1 = unk.state[i];

    // These are the 5 output bits that are calculated for each cell
    uint64_t set_off = 0; // Set an UNKNOWN cell to OFF
    uint64_t set_on = 0;
    uint64_t abort = 0; // The neighbourhood is inconsistent

// Begin Autogenerated
set_off |= on2 ;
set_off |= (~on1) & (~unk1) ;
set_off |= (~on1) & (~on0) & (~unk0) ;
set_on |= (~on2) & on1 & on0 & (~unk1) ;
abort |= state0 & on2 & on1 ;
abort |= state0 & on2 & on0 ;
abort |= (~state0) & (~on2) & on1 & on0 & (~unk1) & (~unk0) ;
abort |= state0 & (~on1) & on0 & (~unk1) ;
abort |= state0 & on1 & (~on0) & (~unk1) & (~unk0) ;
// End Autogenerated

   new_off.state[i] = set_off;
   new_on.state[i] = set_on;

   has_set_off |= set_off;
   has_set_on |= set_on;
   has_abort |= abort;
  }

  if(has_abort != 0)
    return std::make_pair(false, false);

  if (has_set_off != 0) {
    known |= new_off & unk;
  }

  if (has_set_on != 0) {
    stable |= new_on & unk;
    state |= new_on & unk;
    known |= new_on & unk;
  }

  return std::make_pair(has_abort == 0, startKnown == known);
}

bool SearchState::SimplePropagateStable() {
  bool done = false;
  while (!done) {
    auto result = SimplePropagateStableStep();
    if (!result.first) {
      return false;
    }
    done = result.second;
  }
  return true;
}

std::pair<bool, bool> SearchState::PropagateStableStep() {
  LifeState startKnown = known;
  LifeState unk = ~known;

  LifeState oncol0, oncol1, unkcol0, unkcol1;
  CountRows(stable, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);

  LifeState new_off, new_on, new_signal_off, new_signal_on;

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
      on2 |= on3;
      on1 |= on3;
      on0 |= on3;

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
      unk1 |= unk2 | unk3;
      unk0 |= unk2 | unk3;
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
set_off |= (~on1) & (~unk1) ;
set_off |= (~on1) & (~on0) & (~unk0) ;
set_on |= (~on2) & on1 & on0 & (~unk1) ;
signal_off |= (~state1) & (~state0) & (~on2) & on1 & (~on0) & (~unk1) & unk0 ;
signal_off |= state0 & (~on1) & (~on0) & unk1 ;
signal_off |= state0 & (~on1) & (~unk1) & unk0 ;
signal_on |= (~state1) & (~state0) & (~on2) & on1 & on0 & (~unk1) ;
signal_on |= state0 & on1 & (~on0) & (~unk1) ;
signal_on |= state0 & (~on1) & on0 & (~unk0) ;
abort |= state0 & on2 & on1 ;
abort |= state0 & on2 & on0 ;
abort |= (~state0) & (~on2) & on1 & on0 & (~unk1) & (~unk0) ;
abort |= state0 & (~on1) & on0 & (~unk1) ;
abort |= state0 & on1 & (~on0) & (~unk1) & (~unk0) ;
// End Autogenerated

   new_off.state[i] = set_off;
   new_on.state[i] = set_on;
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

  return std::make_pair(has_abort == 0, true);
}

bool SearchState::PropagateStable() {
  return PropagateStableStep().first;
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

void SearchState::UncertainActiveStep(LifeState &next, LifeState &nextUnknown) {
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
      on2 |= on3;
      on1 |= on3;
      on0 |= on3;

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
      unk1 |= unk2 | unk3;
      unk0 |= unk2 | unk3;
    }

    uint64_t stateon = state.state[i];
    uint64_t stateunk = unk.state[i];
    uint64_t statesta = sta.state[i];

    uint64_t next_on = 0;
    uint64_t unknown = 0;

    // ALWAYS CHECK THE PHASE that espresso outputs or you will get confused
// Begin Autogenerated
next_on |= stateon & (~on1) & (~on0) ;
next_on |= (~statesta) & (~on2) & on1 & on0 ;
next_on |= (~on2) & on1 & on0 & (~sta3) ;
next_on |= stateon & statesta & sta3 & sta0 ;
next_on |= (~on2) & on1 & on0 & (~sta0) ;
unknown |= (~stateon) & on2 ;
unknown |= on2 & on1 ;
unknown |= on2 & on0 ;
unknown |= (~unk1) & (~unk0) ;
unknown |= stateunk & statesta & (~unk1) ;
unknown |= stateon & on0 & (~unk1) ;
unknown |= stateunk & on0 & (~unk1) ;
unknown |= statesta & sta3 & sta0 ;
unknown |= (~on2) & (~on1) & (~unk1) ;
unknown |= (~on2) & (~on1) & (~on0) & (~unk0) ;
// End Autogenerated

    next.state[i] = next_on;
    nextUnknown.state[i] = ~unknown; // "phase" in the espresso result is 0
  }
  next.gen = state.gen + 1;
}

void SearchState::UncertainStep(LifeState &next, LifeState &nextUnknown, LifeState &glancing) {
  LifeState result;

  LifeState unk = ~known;

  LifeState oncol0, oncol1, unkcol0, unkcol1;
  CountRows(state, oncol0, oncol1);
  CountRows(unk, unkcol0, unkcol1);

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
      on2 |= on3;
      on1 |= on3;
      on0 |= on3;

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
      unk1 |= unk2 | unk3;
      unk0 |= unk2 | unk3;
    }

    uint64_t stateon = state.state[i];
    uint64_t stateunk = unk.state[i];

    uint64_t next_on = 0;
    uint64_t unknown = 0;

    // ALWAYS CHECK THE PHASE that espresso outputs or you will get confused
// Begin Autogenerated
unknown |= (~on2) & on1 & unk1 ;
unknown |= (~on2) & on0 & unk1 ;
unknown |= (~on2) & unk1 & unk0 ;
unknown |= stateon & (~on1) & (~on0) & unk1 ;
unknown |= stateon & (~on1) & (~on0) & unk0 ;
unknown |= (~stateunk) & (~stateon) & (~on2) & on1 & unk0 ;
unknown |= (~on2) & on1 & (~on0) & unk0 ;
next_on |= stateunk & (~on2) & on1 & on0 & (~unk1) ;
next_on |= stateon & (~on2) & on1 & on0 & (~unk1) ;
next_on |= (~on2) & on1 & on0 & (~unk1) & (~unk0) ;
next_on |= stateon & (~on1) & (~on0) & (~unk1) & (~unk0) ;
// End Autogenerated

    next.state[i] = next_on;
    nextUnknown.state[i] = unknown;
    glancing.state[i] = (~stateon) & (~stateunk) & (~on2) & (~on1) & on0 & (unk0 | unk1);
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
  LifeState best;
  unsigned maxPop = std::numeric_limits<int>::max();
  LifeState searchArea = stable;

  auto startTime = std::chrono::system_clock::now();

  while(!(~known & ~searchArea).IsEmpty()) {
    searchArea = searchArea.ZOI();
    SearchState copy = *this;
    copy.known |= ~searchArea;
    copy.CompleteStable(maxPop, best);

    auto currentTime = std::chrono::system_clock::now();
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

    if (best.GetPop() > 0 || seconds > 10)
      break;
  }
  return best;
}

bool SearchState::CompleteStable(unsigned &maxPop, LifeState &best) {
  // std::cout << stable.RLE() << std::endl;
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
    SearchState nextState = *this;
    nextState.stable.Erase(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    onresult = nextState.CompleteStable(maxPop, best);
  }
  // Then must be on
  {
    SearchState nextState = *this;
    nextState.stable.Set(newPlacement.first, newPlacement.second);
    nextState.known.Set(newPlacement.first, newPlacement.second);
    offresult = nextState.CompleteStable(maxPop, best);
  }

  return onresult || offresult;
}

bool SearchState::RunSearch(SearchParams &params, Focus focus, LifeState& triedGlancing) {
  bool debug = params.debug;

  if(debug) std::cout << "focus: " << focus.type << " (" << focus.coords.first << ", " << focus.coords.second << ")" << std::endl;
  if (!hasInteracted && state.gen > params.maxFirstActiveGen) {
    if(debug) std::cout << "failed: didn't interact before " << params.maxFirstActiveGen << std::endl;
    return false;
  }
  if (hasInteracted && state.gen - interactionStartTime > params.maxActiveWindowGens + params.minStableInterval) {
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
    if(state.gen - interactionStartTime < params.minActiveWindowGens + params.minStableInterval)
      return false;

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

  if (focus.type != NONE) {
    bool consistent = SimplePropagateColumnStep(focus.coords.first);

    if (!consistent) {
      if (debug) std::cout << "failed: inconsistent" << std::endl;
      return false;
    }
  } else {
    bool consistent = SimplePropagateStable();

    if (!consistent) {
      if (debug) std::cout << "failed: inconsistent" << std::endl;
      return false;
    }
  }

  LifeState next;
  LifeState nextUnknowns;
  LifeState glancing;

  //  if (focus.type == NONE) {
    // Find the next unknown cell

  UncertainStep(next, nextUnknowns, glancing);
    // Prevent the unknown zone from growing, as in Bellman
  LifeState uneqStableNbhd = (state ^ stable).ZOI();
  next &= uneqStableNbhd;
  next |= stable & ~uneqStableNbhd;
  nextUnknowns &= uneqStableNbhd;
  nextUnknowns |= ~known & ~uneqStableNbhd;
  nextUnknowns &= ~triedGlancing;
  glancing &= ~triedGlancing;


  if (focus.type == GLANCING && !nextUnknowns.GetCell(focus.coords.first, focus.coords.second) && !next.GetCell(focus.coords.first, focus.coords.second)) {
    // std::cout << "glancing failed:" << std::endl << std::flush;
    // std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
    // std::cout << UnknownRLE() << std::endl << std::flush;
    // LifeState without = state;
    // without.Set(focus.coords.first, focus.coords.second);
    // std::cout << state.RLE() << std::endl << std::flush;
    // std::cout << without.RLE() << std::endl << std::flush;
    // This glancing interaction failed
    return false;
  }

  if (focus.type != NONE && !nextUnknowns.GetCell(focus.coords.first, focus.coords.second)) {
    // Done with this focus
    focus = Focus::None();
  }

  if(focus.type == NONE) {
    // Find unknown cells that were known in the previous generation
    LifeState newUnknowns = nextUnknowns & known;

    auto coords = (newUnknowns & ~glancing).FirstOn();
    if (coords != std::make_pair(-1, -1)) {
      focus = Focus(NORMAL, coords);
    } else {
      coords = (newUnknowns & glancing).FirstOn();
      if (coords != std::make_pair(-1, -1)) {
        focus = Focus(GLANCING, coords);
        {
          // Just ignore any glancing interaction and proceed
          SearchState nextState = *this;

          LifeState newGlancing = triedGlancing;
          newGlancing.Set(focus.coords.first, focus.coords.second);

          bool result = nextState.RunSearch(params, Focus::None(), newGlancing);
        }
      } else {
        focus = Focus::None();
      }
    }
  }

    // auto coords = newUnknowns.FirstOn();
    // if (coords != std::make_pair(-1, -1)) {
    //   focus = Focus(NORMAL, coords);
    // }

  if (focus.type == NORMAL || focus.type == GLANCING) {
    // Set an unknown neighbour of the focus
    std::pair<int, int> unknown = UnknownNeighbour(focus.coords);

    bool whichFirst = false;
    {
      SearchState nextState = *this;

      if(!hasInteracted && whichFirst)
        nextState.preInteractionChoices += 1;
      if(hasInteracted)
        nextState.postInteractionChoices += 1;

      nextState.state.SetCellUnsafe(unknown.first, unknown.second, whichFirst);
      nextState.stable.SetCellUnsafe(unknown.first, unknown.second, whichFirst);
      nextState.known.Set(unknown.first, unknown.second);
      bool result = nextState.RunSearch(params, focus, triedGlancing);
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

      nextState.state.SetCellUnsafe(unknown.first, unknown.second, !whichFirst);
      nextState.stable.SetCellUnsafe(unknown.first, unknown.second, !whichFirst);
      nextState.known.Set(unknown.first, unknown.second);
      bool result = nextState.RunSearch(params, focus, triedGlancing);
      // if (result) {
      //   *this = nextState;
      //   return true;
      // }
    }
    return true;
  } else {
    // We can safely take a step
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

    state = next;
    known = ~nextUnknowns;

    // bool consistent = PropagateStable();
    // if (!consistent) {
    //   if (debug) std::cout << "failed: inconsistent after stepping" << std::endl;
    //   return false;
    // }

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
  }
}

int main(int argc, char *argv[]) {
  auto toml = toml::parse(argv[1]);
  SearchParams params = SearchParams::FromToml(toml);

  SearchState search;
  search.state = params.activePattern;
  search.stable = params.startingStable;
  search.known = ~params.searchArea;

  bool result = search.RunSearch(params);
}
