#pragma once

#include "LifeAPI.h"

// This uses lifelib "layers" which do not match Golly's state names,
// so the parsing has to adjust for this.
struct LifeHistoryState {
  LifeState state;
  LifeState history;
  LifeState marked;
  LifeState original;

  LifeHistoryState() = default;
  LifeHistoryState(LifeState state, LifeState history, LifeState marked, LifeState original)
    : state{state},
      history{history},
      marked{marked},
      original{original}
  {};

  std::string RLE() const;

  void Move(int x, int y) {
    state.Move(x, y);
    history.Move(x, y);
    marked.Move(x, y);
    original.Move(x, y);
  }

  void Move(std::pair<int, int> vec) {
    Move(vec.first, vec.second);
  }
};

inline char LifeHistoryChar(unsigned mask) {
  switch (mask) {
  case 0 + (0 << 1) + (0 << 2) + (0 << 3):
    return '.';
  case 1 + (0 << 1) + (0 << 2) + (0 << 3):
    return 'A';
  case 0 + (1 << 1) + (0 << 2) + (0 << 3):
    return 'B';
  case 1 + (0 << 1) + (1 << 2) + (0 << 3):
    return 'C';
  case 0 + (0 << 1) + (1 << 2) + (0 << 3):
    return 'D';
  case 1 + (0 << 1) + (0 << 2) + (1 << 3):
    return 'E';
  default:
    return 'F';
  }
}

std::string LifeHistoryState::RLE() const {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < 64; j++) {
    unsigned last_val;
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      bool statecell = state.GetCell(i - (N/2), j - 32) == 1;
      bool historycell = history.GetCell(i - (N/2), j - 32) == 1;
      bool markedcell = marked.GetCell(i - (N/2), j - 32) == 1;
      bool originalcell = original.GetCell(i - (N/2), j - 32) == 1;
      unsigned val = statecell + (historycell << 1) + (markedcell << 2) + (originalcell << 3);

      if (i == 0)
        last_val = val;

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
        result << LifeHistoryChar(last_val);
        run_count = 0;
      }

      run_count++;
      last_val = val;
    }

    // Flush run of live cells at end of line
    if (last_val) {
      if (run_count > 1)
        result << run_count;

      result << LifeHistoryChar(last_val);

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
