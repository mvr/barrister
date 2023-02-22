#include "LifeAPI.h"

void ParseTristate(const std::string &rle, LifeState &stateon, LifeState &statemarked) {
  int cnt;
  int x, y;
  x = 0;
  y = 0;
  cnt = 0;

  for (char const ch : rle) {

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

      for (int j = 0; j < cnt; j++) {
        switch(ch) {
        case 'A':
          stateon.Set(x, y);
          break;
        case 'B': case 'E': // For LifeBellman
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
  }
}

void ParseTristateWHeader(const std::string &s, LifeState &stateon, LifeState &statemarked) {
  std::string rle;
  std::istringstream iss(s);

  for (std::string line; std::getline(iss, line); ) {
    if(line[0] != 'x')
      rle += line;
  }

  ParseTristate(rle, stateon, statemarked);
}

std::string MultiStateRLE(const std::array<char, 4> table, const LifeState &state, const LifeState &marked) {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < N; j++) {
    unsigned last_val = (state.GetCell(0 - 32, j - 32) == 1) + ((marked.GetCell(0 - 32, j - 32) == 1) << 1);
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      unsigned val = (state.GetCell(i - 32, j - 32) == 1) + ((marked.GetCell(i - 32, j - 32) == 1) << 1);

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
        result << table[last_val];
        run_count = 0;
      }

      run_count++;
      last_val = val;
    }

    // Flush run of live cells at end of line
    if (last_val) {
      if (run_count > 1)
        result << run_count;

      result << table[last_val];

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

std::string UnknownRLEFor(const LifeState &stable, const LifeState &unknown) {
  return MultiStateRLE({'.', 'A', 'B', 'Q'}, stable, unknown);
}

std::string LifeBellmanRLEFor(const LifeState &state, const LifeState &marked) {
  return MultiStateRLE({'.', 'A', 'E', 'C'}, state, marked);
}
