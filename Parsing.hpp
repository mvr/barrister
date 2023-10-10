#pragma once

#include "LifeAPI.h"

std::string GenericRLE(auto&& cellchar) {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < 64; j++) {
    char last_val = cellchar((0 - (N / 2) + N) % N, ((signed)j - 32 + 64) % 64);
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      char val = cellchar(((signed)i - (N / 2) + N) % N, ((signed)j - 32 + 64) % 64);

      // Flush linefeeds if we find a live cell
      if (val != '.' && val != 'b' && eol_count > 0) {
        if (eol_count > 1)
          result << eol_count;

        result << "$";

        eol_count = 0;
      }

      // Flush current run if val changes
      if (val != last_val) {
        if (run_count > 1)
          result << run_count;
        result << last_val;
        run_count = 0;
      }

      run_count++;
      last_val = val;
    }

    // Flush run of live cells at end of line
    if (last_val != '.' && last_val != 'b') {
      if (run_count > 1)
        result << run_count;

      result << last_val;

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

std::string LifeBellmanRLEFor(const LifeState &state, const LifeState &marked) {
  return GenericRLE([&](int x, int y) -> char {
    const std::array<char, 4> table = {'.', 'A', 'E', 'C'};
    return table[state.Get(x, y) + (marked.Get(x, y) << 1)];
  });
}

std::string RowRLE(std::vector<LifeState> &row) {
  const unsigned spacing = 70;

  std::stringstream result;

  bool last_val;
  unsigned run_count = 0;
  unsigned eol_count = 0;
  for (unsigned j = 0; j < spacing; j++) {
    if(j < 64)
      last_val = row[0].GetSafe(0 - N/2, j - 32);
    else
      last_val = false;
    run_count = 0;

    for (auto &pat : row) {
      for (unsigned i = 0; i < spacing; i++) {
        bool val = false;
        if(i < N && j < 64)
          val = pat.GetSafe(i - N/2, j - 32);

        // Flush linefeeds if we find a live cell
        if (val && eol_count > 0) {
          if (eol_count > 1)
            result << eol_count;

          result << "$";

          eol_count = 0;
        }

        // Flush current run if val changes
        if (val == !last_val) {
          if (run_count > 1)
            result << run_count;

          if (last_val == 1)
            result << "o";
          else
            result << "b";

          run_count = 0;
        }

        run_count++;
        last_val = val;
      }
    }
    // Flush run of live cells at end of line
    if (last_val) {
      if (run_count > 1)
        result << run_count;

      result << "o";

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
