#pragma once

#include "LifeAPI.h"
#include "Parsing.hpp"

// This uses lifelib "layers" which do not match Golly's state names,
// so the parsing has to adjust for this.
struct LifeHistoryState {
  LifeState state;
  LifeState history;
  LifeState marked;
  LifeState original;

  LifeHistoryState() = default;
  LifeHistoryState(const LifeState &state, const LifeState &history,
                   const LifeState &marked, const LifeState &original)
      : state{state}, history{history}, marked{marked}, original{original} {};
  LifeHistoryState(const LifeState &state, const LifeState &history, const LifeState &marked)
      : state{state}, history{history}, marked{marked}, original{LifeState()} {};
  LifeHistoryState(const LifeState &state, const LifeState &history)
      : state{state}, history{history}, marked{LifeState()},
        original{LifeState()} {};

  std::string RLE() const;
  std::string RLEWHeader() const {
    return "x = 0, y = 0, rule = LifeHistory\n" + RLE();
  }
  friend std::ostream& operator<<(std::ostream& os, LifeHistoryState const& self) {
    return os << self.RLEWHeader();
  }

  static LifeHistoryState Parse(const std::string &s);
  static LifeHistoryState ParseWHeader(const std::string &s);

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

char LifeHistoryChar(unsigned mask) {
  switch (mask) {
  case 0b0000: return '.';
  case 0b0001: return 'A';
  case 0b0010: return 'B';
  case 0b0101: return 'C';
  case 0b0100: return 'D';
  case 0b1001: return 'E';
  default:     return 'F';
  }
}

std::string LifeHistoryState::RLE() const {
  return GenericRLE([&](int x, int y) -> char {
    bool statecell = state.Get(x, y) == 1;
    bool historycell = history.Get(x, y) == 1;
    bool markedcell = marked.Get(x, y) == 1;
    bool originalcell = original.Get(x, y) == 1;
    unsigned val = statecell + (historycell << 1) + (markedcell << 2) + (originalcell << 3);

    return LifeHistoryChar(val);
  });
}

LifeHistoryState LifeHistoryState::Parse(const std::string &rle) {
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
    } else if (ch == '\n' || ch == ' ') {
      continue;
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

LifeHistoryState LifeHistoryState::ParseWHeader(const std::string &s) {
  std::string rle;
  std::istringstream iss(s);

  for (std::string line; std::getline(iss, line); ) {
    if(line[0] != 'x')
      rle += line;
  }

  return Parse(rle);
}
