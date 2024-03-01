#pragma once

#include <algorithm>
#include <array>

#define N 64

enum SymmetryTransform {
  Identity,
  ReflectAcrossXEven,
  ReflectAcrossX,
  ReflectAcrossYEven,
  ReflectAcrossY,
  Rotate90Even,
  Rotate90,
  Rotate270Even,
  Rotate270,
  Rotate180OddBoth,
  Rotate180EvenHorizontal,
  Rotate180EvenVertical,
  Rotate180EvenBoth,
  ReflectAcrossYeqX,
  ReflectAcrossYeqNegX,
  // reflect across y = -x+3/2, fixing (0,0), instead of y=-x+1/2,
  // sending (0,0) to (-1,-1). Needed for D4x_1 symmetry.
  ReflectAcrossYeqNegXP1
};

enum struct StaticSymmetryType {
  C1,
  D2AcrossX,
  D2AcrossY,
  D2diagodd,
  D2negdiagodd,
  C2,
  C4,
  D4,
  D4diag,
  D8,
};

enum struct StaticSymmetryParityType {
  C1,
  D2AcrossX,
  D2AcrossXEven,
  D2AcrossY,
  D2AcrossYEven,
  D2negdiagodd,
  D2diagodd,
  C2,
  C2even,
  C2verticaleven,
  C2horizontaleven,
  C4,
  C4even,
  D4,
  D4even,
  D4verticaleven,
  D4horizontaleven,
  D4diag,
  D4diageven,
  D8,
  D8even,
};


struct StaticSymmetry {
  StaticSymmetryType type;
  std::pair<int, int> offset;

  bool operator==(const StaticSymmetry&) const = default;

  std::pair<int, int> Center();
};


inline std::pair<int, int> StaticSymmetry::Center(){
  switch (type) {
  case StaticSymmetryType::C4: { // This is the center of rotation for offset rotation by 90
    int x = offset.first;
    int y = offset.second;
    int x2 = (x - y) / 2;
    int y2 = (x + y) / 2;
    int x3 = ((x2 + 16 + 32) % 32 - 16 + 64) % 64;
    int y3 = ((y2 + 16 + 32) % 32 - 16 + 64) % 64;
    return std::make_pair(x3, y3);
  }
  default: {
    int x = (((offset.first + 32) % 64 - 32) / 2 + 64) % 64;
    int y = (((offset.second + 32) % 64 - 32) / 2 + 64) % 64;
    return std::make_pair(x, y);
  }
  }
}

std::pair<unsigned, std::array<std::pair<int, int>, 8>> OrbitUnsafe(std::pair<int, int> cell, StaticSymmetry sym) {
  auto [x, y] = cell;
  switch (sym.type) {
  case StaticSymmetryType::C1: return {1, {std::make_pair(x, y)}};
  case StaticSymmetryType::C2: return {2, {std::make_pair(x, y), std::make_pair(-x+sym.offset.first, -y+sym.offset.second)}};
  case StaticSymmetryType::D2AcrossX:
  case StaticSymmetryType::D2AcrossY:
  case StaticSymmetryType::D2diagodd:
  case StaticSymmetryType::D2negdiagodd:
  case StaticSymmetryType::C4:
  case StaticSymmetryType::D4:
  case StaticSymmetryType::D4diag:
  case StaticSymmetryType::D8:
    return {1, {std::make_pair(x, y)}};
  }
}

// std::pair<unsigned, std::array<std::pair<int, int>, 8>> OrbitUnsafe(std::pair<int, int> cell, StaticSymmetry sym) {
//   auto [x, y] = cell;
//   auto [count, array] = OrbitUnsafe(cell, sym);
//   for (unsigned i = 0; i < count; i++)
//     array[i] =
//   return {count, array};
// }

std::pair<unsigned, std::array<std::pair<int, int>, 8>> Orbit(std::pair<int, int> cell, StaticSymmetry sym) {
  auto [count, array] = OrbitUnsafe(cell, sym);
  for (unsigned i = 0; i < count; i++)
    array[i] = {(array[i].first + N) % N, (array[i].second + 64) % 64};
  return {count, array};
}

inline std::pair<int, int> PerpComponent(SymmetryTransform transf,
                                         std::pair<int, int> offset) {
  switch (transf) {
  case ReflectAcrossX:
    return std::make_pair(0, offset.second);
  case ReflectAcrossY:
    return std::make_pair(offset.first, 0);
  case ReflectAcrossYeqX: {
    int x = (offset.first + 32) % 64 - 32;
    int y = (offset.second + 32) % 64 - 32;
    return std::make_pair(((x - y + 128) / 2) % 64,
                          ((-x + y + 128) / 2) % 64);
  }
  case ReflectAcrossYeqNegXP1: {
    int x = (offset.first + 32) % 64 - 32;
    int y = (offset.second + 32) % 64 - 32;
    return std::make_pair(((x + y + 128) / 2) % 64,
                          ((x + y + 128) / 2) % 64);
  }
  default:
    return offset;
  }
}
