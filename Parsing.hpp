#pragma once

#include "LifeAPI.h"
#include "LifeHistoryState.hpp"

LifeHistoryState ParseLifeHistory(const std::string &rle) {
  LifeHistoryState result;

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
        return result;

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
          result.state.Set(x, y);
          break;
        case 'B':
          result.history.Set(x, y);
          break;
        case 'C':
          result.state.Set(x, y);
          result.marked.Set(x, y);
          break;
        case 'D':
          result.marked.Set(x, y);
          break;
        case 'E':
          result.state.Set(x, y);
          result.original.Set(x, y);
          break;
        }
        x++;
      }

      cnt = 0;
    }
  }
  return result;
}

LifeHistoryState ParseLifeHistoryWHeader(const std::string &s) {
  std::string rle;
  std::istringstream iss(s);

  for (std::string line; std::getline(iss, line); ) {
    if(line[0] != 'x')
      rle += line;
  }

  return ParseLifeHistory(rle);
}

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

  for (unsigned j = 0; j < 64; j++) {
    unsigned last_val = (state.GetCell(0 - (N/2), j - 32) == 1) + ((marked.GetCell(0 - (N/2), j - 32) == 1) << 1);
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      unsigned val = (state.GetCell(i - (N/2), j - 32) == 1) + ((marked.GetCell(i - (N/2), j - 32) == 1) << 1);

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

std::string RowRLE(std::vector<LifeState> &row) {
  const unsigned spacing = 70;

  std::stringstream result;

  bool last_val;
  unsigned run_count = 0;
  unsigned eol_count = 0;
  for (unsigned j = 0; j < spacing; j++) {
    if(j < 64)
      last_val = row[0].GetCell(0 - N/2, j - 32);
    else
      last_val = false;
    run_count = 0;

    for (auto &pat : row) {
      for (unsigned i = 0; i < spacing; i++) {
        bool val = false;
        if(i < N && j < 64)
          val = pat.GetCell(i - N/2, j - 32);

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
