// LifeAPI provide comfortable functions (API) to manipulate, iterate, evolve,
// compare and report Life objects. This is mainly done in order to provide fast
// (using C) but still comfortable search utility. Contributor Chris Cain.
// Written by Michael Simkin 2014

#pragma once

#include <algorithm>
#include <array>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <random>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Best if multiple of 4
#define N 32

#define SUCCESS 1
#define FAIL 0

// GCC
#ifdef __GNUC__
#ifndef __clang__
#include <x86intrin.h>

uint64_t reverse_uint64_t(uint64_t x) {
  const uint64_t h1 = 0x5555555555555555ULL;
  const uint64_t h2 = 0x3333333333333333ULL;
  const uint64_t h4 = 0x0F0F0F0F0F0F0F0FULL;
  const uint64_t v1 = 0x00FF00FF00FF00FFULL;
  const uint64_t v2 = 0x0000FFFF0000FFFFULL;
  x = ((x >> 1) & h1) | ((x & h1) << 1);
  x = ((x >> 2) & h2) | ((x & h2) << 2);
  x = ((x >> 4) & h4) | ((x & h4) << 4);
  x = ((x >> 8) & v1) | ((x & v1) << 8);
  x = ((x >> 16) & v2) | ((x & v2) << 16);
  x = (x >> 32) | (x << 32);
  return x;
}

#define __builtin_rotateleft64 __rolq
#define __builtin_rotateright64 __rorq
#define __builtin_bitreverse64 ::reverse_uint64_t
#endif
#endif

// MSVC
#ifdef __MSC_VER
#include <intrin.h>
#define __builtin_popcount __popcnt64
#define __builtin_rotateleft64 _rotl64
#define __builtin_rotateright64 _rotr64

inline int __builtin_ctzll(uint64_t x) {
  unsigned long log2;
  _BitScanReverse64(&log2, x);
  return log2;
}
#endif

inline unsigned longest_run_uint64_t(uint64_t x) {
  if(x == 0)
    return 0;

  unsigned count = 0;

  for(int n = 5; n >= 0; n--) {
    for(unsigned i = 1; i <= (1<<n); i *= 2) {
      uint64_t y = (x & __builtin_rotateleft64(x, i));
      if(y != 0) {
        x = y;
        count += i;
      }
    }
  }

  return count + 1;
}

inline unsigned populated_width_uint64_t(uint64_t x) {
  if (x == 0)
    return 0;

  // First, shift to try and make it 2^n-1
  int lzeroes = __builtin_ctzll(x);
  x = __builtin_rotateright64(x, lzeroes);
  int tones = __builtin_clzll(~x);
  x = __builtin_rotateleft64(x, tones);

  if ((x & (x + 1)) == 0)
    return __builtin_ctzll(~x);

  // Otherwise do the long way
  return 64 - longest_run_uint64_t(~x);
}

inline unsigned longest_run_uint32_t(uint32_t x) {
  if(x == 0)
    return 0;

  unsigned count = 0;

  for(int n = 4; n >= 0; n--) {
    for(unsigned i = 1; i <= (1<<n); i *= 2) {
      uint32_t y = (x & __builtin_rotateleft32(x, i));
      if(y != 0) {
        x = y;
        count += i;
      }
    }
  }

  return count + 1;
}

inline unsigned populated_width_uint32_t(uint32_t x) {
  if (x == 0)
    return 0;

  // First, shift to try and make it 2^n-1
  int lzeroes = __builtin_ctzll(x);
  x = __builtin_rotateright32(x, lzeroes);
  int tones = __builtin_clzll(~x);
  x = __builtin_rotateleft32(x, tones);

  if ((x & (x + 1)) == 0)
    return __builtin_ctzll(~x);

  return 32 - longest_run_uint32_t(~x);
}

inline uint64_t convolve_uint64_t(uint64_t x, uint64_t y) {
  if(y == 0)
    return 0;

  uint64_t result = 0;
  while (x != 0) {
    int lsb = __builtin_ctzll(x);
    result |= __builtin_rotateleft64(y, lsb);
    x &= ~(((uint64_t)1) << lsb);
  }
  return result;
}

namespace PRNG {
  std::random_device rd;
  std::mt19937_64 e2(rd());
  std::uniform_int_distribution<uint64_t> dist(std::llround(std::pow(2,61)), std::llround(std::pow(2,62)));
// Public domain PRNG by Sebastian Vigna 2014, see http://xorshift.di.unimi.it

uint64_t s[16] = {0x12345678};
int p = 0;

uint64_t rand64() {
  uint64_t s0 = s[p];
  uint64_t s1 = s[p = (p + 1) & 15];
  s1 ^= s1 << 31; // a
  s1 ^= s1 >> 11; // b
  s0 ^= s0 >> 30; // c
  return (s[p] = s0 ^ s1) * 1181783497276652981ULL;
}

} // namespace PRNG

// void fastMemcpy(void *pvDest, void *pvSrc, size_t nBytes) {
//   assert(nBytes % 32 == 0);
//   assert((intptr_t(pvDest) & 31) == 0);
//   assert((intptr_t(pvSrc) & 31) == 0);
//   const __m256i *pSrc = reinterpret_cast<const __m256i*>(pvSrc);
//   __m256i *pDest = reinterpret_cast<__m256i*>(pvDest);
//   int64_t nVects = nBytes / sizeof(*pSrc);
//   for (; nVects > 0; nVects--, pSrc++, pDest++) {
//     const __m256i loaded = _mm256_stream_load_si256(pSrc);
//     _mm256_stream_si256(pDest, loaded);
//   }
//   _mm_sfence();
// }

enum CopyType { COPY, OR, XOR, AND, ANDNOT, ORNOT };

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

enum StaticSymmetry {
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

inline uint64_t RotateLeft(uint64_t x, unsigned int k) {
  return __builtin_rotateleft64(x, k);
}

inline uint64_t RotateRight(uint64_t x, unsigned int k) {
  return __builtin_rotateright64(x, k);
}

inline uint64_t RotateLeft(uint64_t x) { return RotateLeft(x, 1); }
inline uint64_t RotateRight(uint64_t x) { return RotateRight(x, 1); }

class LifeTarget;

class LifeState {
public:
  uint64_t state[N];

  LifeState() : state{0} {}
  LifeState(bool dummy) {}

  void Set(int x, int y) { state[x] |= (1ULL << (y)); }
  void Erase(int x, int y) { state[x] &= ~(1ULL << (y)); }
  int Get(int x, int y) const { return (state[x] & (1ULL << y)) >> y; }
  void SetCell(int x, int y, int val) {
    if (val == 1) {
      Set((x + N) % N, (y + 64) % 64);
    }
    if (val == 0)
      Erase((x + N) % N, (y + 64) % 64);
  }
  void SetCellUnsafe(int x, int y, int val) {
    if (val == 1)
      Set(x, y);
    if (val == 0)
      Erase(x, y);
  }
  int GetCell(int x, int y) const {
    return Get((x + N) % N, (y + 64) % 64);
  }

  void Set(std::pair<int, int> xy) { Set(xy.first, xy.second); };
  void Erase(std::pair<int, int> xy) { Erase(xy.first, xy.second); };
  int Get(std::pair<int, int> xy) const { return Get(xy.first, xy.second); };
  void SetCell(std::pair<int, int> xy, int val) { SetCell(xy.first, xy.second, val); };
  void SetCellUnsafe(std::pair<int, int> xy, int val) { SetCellUnsafe(xy.first, xy.second, val); };
  int GetCell(std::pair<int, int> xy) const { return GetCell(xy.first, xy.second); };

  uint64_t& operator[](unsigned i) { return state[i]; }
  const uint64_t& operator[](unsigned i) const { return state[i]; }

  uint64_t GetHash() const {
    uint64_t result = 0;

    for (int i = 0; i < N; i++) {
      result = (result + RotateLeft(result)) ^ state[i];
    }

    return result;
  }

public:
  void Print() const;

  void Copy(const LifeState &delta, CopyType op) {
    if (op == COPY) {
      for (int i = 0; i < N; i++)
        state[i] = delta[i];
      return;
    }
    if (op == OR) {
      for (int i = 0; i < N; i++)
        state[i] |= delta[i];
      return;
    }
    if (op == AND) {
      for (int i = 0; i < N; i++)
        state[i] &= delta[i];
    }
    if (op == ANDNOT) {
      for (int i = 0; i < N; i++)
        state[i] &= ~delta[i];
    }
    if (op == ORNOT) {
      for (int i = 0; i < N; i++)
        state[i] |= ~delta[i];
    }
    if (op == XOR) {
      for (int i = 0; i < N; i++)
        state[i] ^= delta[i];
    }
  }

  void Copy(const LifeState &delta) { Copy(delta, COPY); }

  inline void Copy(const LifeState &delta, int x, int y) {
    uint64_t temp1[N] = {0};

    if (x < 0)
      x += N;
    if (y < 0)
      y += 64;

    for (int i = 0; i < N; i++)
      temp1[i] = RotateLeft(delta[i], y);

    memmove(state, temp1 + (N - x), x * sizeof(uint64_t));
    memmove(state + x, temp1, (N - x) * sizeof(uint64_t));

  }

  void Join(const LifeState &delta) { Copy(delta, OR); }

  void Join(const LifeState &delta, int x, int y) {
    uint64_t temp[2*N] = {0};

    if (x < 0)
      x += N;
    if (y < 0)
      y += 64;

    for (int i = 0; i < N; i++) {
      temp[i]   = RotateLeft(delta[i], y);
      temp[i+N] = RotateLeft(delta[i], y);
    }

    const int shift = N - x;
    for (int i = 0; i < N; i++) {
      state[i] |= temp[i+shift];
    }
  }

  void JoinWSymChain(const LifeState &state, int x, int y,
                     const std::vector<SymmetryTransform> &symChain) {
    // instead of passing in the symmetry group {id, g_1, g_2,...g_n} and
    // applying each to default orientation we pass in a "chain" of symmetries
    // {h_1, ...h_n-1} that give the group when "chained together": g_j =
    // product of h_1 thru h_j that way, we don't need to initialize a new
    // LifeState for each symmetry.

    LifeState transformed = state;
    transformed.Move(x, y);

    for (auto sym : symChain) {
      LifeState soFar = transformed;
      soFar.Transform(sym);
      transformed.Join(soFar);
    }
    Join(transformed);
  }

  void JoinWSymChain(const LifeState &state,
                     const std::vector<SymmetryTransform> &symChain) {
    LifeState transformed = state;

    for (auto sym : symChain) {
      LifeState soFar = transformed;
      soFar.Transform(sym);
      transformed.Join(soFar);
    }
    Join(transformed);
  }

  unsigned GetPop() const {
    unsigned pop = 0;

    for (int i = 0; i < N; i++) {
      pop += __builtin_popcountll(state[i]);
    }

    return pop;
  }

  std::vector<std::pair<int, int>> OnCells() const;

  unsigned CountNeighbours(std::pair<int, int> cell) const {
    int result = 0;
    const std::vector<std::pair<int, int>> directions = {{-1,0}, {0,-1}, {1,0}, {0,1}, {-1,-1}, {-1, 1}, {1, -1}, {1, 1}};
    for (auto d : directions) {
      int x = (cell.first + d.first + N) % N;
      int y = (cell.second + d.second + N) % N;
      if (GetCell(x, y)) result++;
    }
    return result;
  }



  // bool IsEmpty() const {
  //   for (int i = 0; i < N; i++) {
  //     if(state[i] != 0)
  //       return false;
  //   }

  //   return true;
  // }

  bool IsEmpty() const {
    uint64_t all = 0;
    for (int i = 0; i < N; i++) {
      all |= state[i];
    }

    return all == 0;
  }

  void Inverse() {
    for (int i = 0; i < N; i++) {
      state[i] = ~state[i];
    }
  }

  bool operator==(const LifeState &b) const {
    uint64_t diffs = 0;

    for (int i = 0; i < N; i++)
      diffs |= state[i] ^ b[i];

    return diffs == 0;
  }

  bool operator!=(const LifeState &b) const {
    return !(*this == b);
  }

  LifeState operator~() const {
    LifeState result(false);
    for (int i = 0; i < N; i++) {
      result[i] = ~state[i];
    }
    return result;
  }

  LifeState operator&(const LifeState &other) const {
    LifeState result(false);
    for (int i = 0; i < N; i++) {
      result[i] = state[i] & other[i];
    }
    return result;
  }

  LifeState& operator&=(const LifeState &other) {
    for (int i = 0; i < N; i++) {
      state[i] = state[i] & other[i];
    }
    return *this;
  }

  LifeState operator|(const LifeState &other) const {
    LifeState result(false);
    for (int i = 0; i < N; i++) {
      result[i] = state[i] | other[i];
    }
    return result;
  }

  LifeState& operator|=(const LifeState &other) {
    for (int i = 0; i < N; i++) {
      state[i] = state[i] | other[i];
    }
    return *this;
  }

  LifeState operator^(const LifeState &other) const {
    LifeState result(false);
    for (int i = 0; i < N; i++) {
      result[i] = state[i] ^ other[i];
    }
    return result;
  }

  LifeState& operator^=(const LifeState &other) {
    for (int i = 0; i < N; i++) {
      state[i] = state[i] ^ other[i];
    }
    return *this;
  }

  inline bool AreDisjoint(const LifeState &pat) const {
    int min = 0;
    int max = N - 1;

    uint64_t differences = 0;
    #pragma clang loop vectorize(enable)
    for (int i = min; i <= max; i++) {
      uint64_t difference = (~state[i] & pat[i]) ^ (pat[i]);
      differences |= difference;
    }

    return differences == 0;
  }

  inline bool Contains(const LifeState &pat) const {
    int min = 0;
    int max = N - 1;

    uint64_t differences = 0;
    #pragma clang loop vectorize(enable)
    for (int i = min; i <= max; i++) {
      uint64_t difference = (state[i] & pat[i]) ^ (pat[i]);
      differences |= difference;
    }

    return differences == 0;
  }

  bool Contains(const LifeState &pat, int targetDx, int targetDy) const {
    int dy = (targetDy + 64) % 64;

    for (int i = 0; i < N; i++) {
      int curX = (N + i + targetDx) % N;

      if ((RotateRight(state[curX], dy) & pat[i]) != (pat[i]))
        return false;
    }
    return true;
  }

  bool AreDisjoint(const LifeState &pat, int targetDx, int targetDy) const {
    int dy = (targetDy + 64) % 64;

    for (int i = 0; i < N; i++) {
      int curX = (N + i + targetDx) % N;

      if (((~RotateRight(state[curX], dy)) & pat[i]) != pat[i])
        return false;
    }

    return true;
  }

  inline bool Contains(const LifeTarget &target, int dx, int dy) const;
  inline bool Contains(const LifeTarget &target) const;

  void Reverse(int idxS, int idxE) {
    for (int i = 0; idxS + 2*i < idxE; i++) {
      int l = idxS + i;
      int r = idxE - i;

      uint64_t temp = state[l];
      state[l] = state[r];
      state[r] = temp;
    }
  }

  void Move(int x, int y) {
    uint64_t temp[2*N] = {0};

    if (x < 0)
      x += N;
    if (y < 0)
      y += 64;

    for (int i = 0; i < N; i++) {
      temp[i]   = RotateLeft(state[i], y);
      temp[i+N] = RotateLeft(state[i], y);
    }

    const int shift = N - x;
    for (int i = 0; i < N; i++) {
      state[i] = temp[i+shift];
    }
  }
  void Move(std::pair<int, int> vec) {
    Move(vec.first, vec.second);
  }

  void BitReverse() {
    for (int i = 0; i < N; i++) {
      state[i] = __builtin_bitreverse64(state[i]);
    }
  }

  void FlipY() { // even reflection across y-axis, ie (0,0) maps to (0, -1)
    Reverse(0, N - 1);
  }

  void Transpose(bool whichDiagonal) {
    int j, k;
    uint64_t m, t;

    for (j = 32, m = 0x00000000FFFFFFFF; j; j >>= 1, m ^= m << j) {
      for (k = 0; k < 64; k = ((k | j) + 1) & ~j) {
        if (whichDiagonal) {
          t = (state[k] ^ (state[k | j] >> j)) & m;
          state[k] ^= t;
          state[k | j] ^= (t << j);
        } else {
          t = (state[k] >> j ^ (state[k | j])) & m;
          state[k] ^= (t << j);
          state[k | j] ^= t;
        }
      }
    }
  }

  void Transpose() { Transpose(true); }

  // even reflection across x-axis, ie (0,0) maps to (0, -1)
  void FlipX() { BitReverse(); }

  void Transform(SymmetryTransform transf);

  void Transform(int dx, int dy, SymmetryTransform transf) {
    Move(dx, dy);
    Transform(transf);
  }

  LifeState ZOI() const {
    LifeState temp(false);
    for (int i = 0; i < N; i++) {
      uint64_t col = state[i];
      temp[i] = col | RotateLeft(col) | RotateRight(col);
    }

    LifeState boundary(false);

    boundary[0] = temp[N-1] | temp[0] | temp[1];
    for(int i = 1; i < N-1; i++)
        boundary[i] = temp[i-1] | temp[i] | temp[i+1];
    boundary[N-1] = temp[N-2] | temp[N-1] | temp[0];

    return boundary;
  }

  LifeState MooreZOI() const {
    LifeState temp(false);
    LifeState boundary(false);
    for (int i = 0; i < N; i++) {
      uint64_t col = state[i];
      temp[i] = col | RotateLeft(col) | RotateRight(col);
    }

    boundary[0] = state[N - 1] | temp[0] | state[1];

    for (int i = 1; i < N - 1; i++)
      boundary[i] = state[i - 1] | temp[i] | state[i + 1];

    boundary[N - 1] = state[N - 2] | temp[N - 1] | state[0];

    return boundary;
  }


  LifeState GetBoundary() const {
    LifeState boundary = ZOI();
    boundary.Copy(*this, ANDNOT);
    return boundary;
  }

  LifeState BigZOI() const {
    LifeState b(false);
    b[0] = state[0] | RotateLeft(state[0]) | RotateRight(state[0]) |
                 state[N - 1] | state[0 + 1];
    for (int i = 1; i < N-1; i++) {
      b[i] = state[i] | RotateLeft(state[i]) | RotateRight(state[i]) | state[i-1] | state[i+1];
    }
    b[N-1] = state[N-1] | RotateLeft(state[N-1]) | RotateRight(state[N-1]) |
                 state[N-1 - 1] | state[0];

    LifeState c(false);
    c[0] = b[0] | b[N - 1] | b[0 + 1];
    for (int i = 1; i < N - 1; i++) {
      c[i] = b[i] | b[i - 1] | b[i + 1];
    }
    c[N - 1] = b[N - 1] | b[N - 1 - 1] | b[0];

    LifeState zoi(false);

    zoi[0] =
      c[0] | RotateLeft(c[0]) | RotateRight(c[0]);
    for (int i = 1; i < N - 1; i++) {
      zoi[i] =
        c[i] | RotateLeft(c[i]) | RotateRight(c[i]);
    }
    zoi[N - 1] = c[N - 1] | RotateLeft(c[N - 1]) |
      RotateRight(c[N - 1]);

    return zoi;
  }

  static inline void ConvolveInner(LifeState &result, const uint64_t (&doubledother)[N*2], uint64_t x, unsigned int k, unsigned int postshift) {
    for (int i = 0; i < N; i++) {
      result[i] |= __builtin_rotateleft64(convolve_uint64_t(x, doubledother[i+k]), postshift);
    }
  }

  LifeState Convolve(const LifeState &other) const {
    LifeState result;
    uint64_t doubledother[N*2];
    memcpy(doubledother,     other.state, N * sizeof(uint64_t));
    memcpy(doubledother + N, other.state, N * sizeof(uint64_t));

    for (unsigned j = 0; j < N; j++) {
      unsigned k = N-j;
      uint64_t x = state[j];

      // Annoying special case
      if(x == ~0ULL) {
        ConvolveInner(result, doubledother, ~0ULL, k, 0);
        continue;
      }

    while (x != 0) {
      unsigned int postshift;

      uint64_t shifted;

      if((x & 1) == 0) { // Possibly wrapped
        int lsb = __builtin_ctzll(x);
        shifted = __builtin_rotateright64(x, lsb);
        postshift = lsb;
      } else{
        int lead = __builtin_clzll(~x);
        shifted = __builtin_rotateleft64(x, lead);
        postshift = 64-lead;
      }

      unsigned runlength = __builtin_ctzll(~shifted);
      runlength = std::min(runlength, (unsigned)32);
      uint64_t run = (1ULL << runlength) - 1;

      switch(run) {
      case (1 << 1) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 2) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 3) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 4) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 5) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 6) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 7) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 8) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 9) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 10) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 11) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 12) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 13) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 14) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 15) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 16) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 17) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 18) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 19) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 20) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 21) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 22) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 23) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 24) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 25) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 26) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 27) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 28) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 29) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1 << 30) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1ULL << 31) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      case (1ULL << 32) - 1: ConvolveInner(result, doubledother, run, k, postshift); break;
      default:           ConvolveInner(result, doubledother, run, k, postshift); break;
      }

      x &= ~__builtin_rotateleft64(run, postshift);
    }
    }

    return result;
  }

  void Clear() {
    for (int i = 0; i < N; i++)
      state[i] = 0;
  }

  LifeState MatchLive(const LifeState &live) const {
    LifeState invThis = ~*this;
    LifeState flipLive = live;
    flipLive.Transform(Rotate180OddBoth);
    return ~invThis.Convolve(flipLive);
  }

  LifeState MatchLiveAndDead(const LifeState &live, const LifeState &dead) const {
    LifeState invThis = ~*this;
    LifeState flipLive = live;
    flipLive.Transform(Rotate180OddBoth);
    LifeState flipDead = dead;
    flipDead.Transform(Rotate180OddBoth);
    return ~invThis.Convolve(flipLive) & ~Convolve(flipDead);
  }

  LifeState Match(const LifeState &live) const {
    return MatchLiveAndDead(live, live.GetBoundary());
  }

  LifeState Match(const LifeTarget &target) const;

  std::pair<int, int> FindSetNeighbour(std::pair<int, int> cell) const;
  unsigned NeighbourhoodCount(std::pair<int, int> cell) const;

private:
  void inline Add(uint64_t &b1, uint64_t &b0, const uint64_t &val) {
    b1 |= b0 & val;
    b0 ^= val;
  }

  void inline Add(uint64_t &b2, uint64_t &b1, uint64_t &b0,
                  const uint64_t &val) {
    uint64_t t_b2 = b0 & val;

    b2 |= t_b2 & b1;
    b1 ^= t_b2;
    b0 ^= val;
  }

  uint64_t inline Evolve(const uint64_t &temp, const uint64_t &bU0,
                         const uint64_t &bU1, const uint64_t &bB0,
                         const uint64_t &bB1) {
    uint64_t sum0 = RotateLeft(temp);

    uint64_t sum1 = 0;
    Add(sum1, sum0, RotateRight(temp));
    Add(sum1, sum0, bU0);

    uint64_t sum2 = 0;
    Add(sum2, sum1, bU1);
    Add(sum2, sum1, sum0, bB0);
    Add(sum2, sum1, bB1);

    return ~sum2 & sum1 & (temp | sum0);
  }

  // From Page 15 of
  // https://www.gathering4gardner.org/g4g13gift/math/RokickiTomas-GiftExchange-LifeAlgorithms-G4G13.pdf
  uint64_t inline Rokicki(const uint64_t &a, const uint64_t &bU0,
                          const uint64_t &bU1, const uint64_t &bB0,
                          const uint64_t &bB1) {
    uint64_t aw = RotateLeft(a);
    uint64_t ae = RotateRight(a);
    uint64_t s0 = aw ^ ae;
    uint64_t s1 = aw & ae;
    uint64_t ts0 = bB0 ^ bU0;
    uint64_t ts1 = (bB0 & bU0) | (ts0 & s0);
    return (bB1 ^ bU1 ^ ts1 ^ s1) & ((bB1 | bU1) ^ (ts1 | s1)) &
           ((ts0 ^ s0) | a);
  }

public:
  void Step();

  void Step(int numIters) {
    for (int i = 0; i < numIters; i++) {
      Step();
      // RemoveGliders();
    }
  }

  static int Parse(LifeState &state, const char *rle, int starti);

  static int Parse(LifeState &state, const char *rle, int dx, int dy) {
    if (Parse(state, rle, 0) == -1) {
      state.Move(dx, dy);
      return SUCCESS;
    } else {
      return FAIL;
    }
  }

  static int Parse(LifeState &state, const char *rle) {
    return Parse(state, rle, 0, 0);
  }

  static int Parse(LifeState &state, const char *rle, int dx, int dy,
                   SymmetryTransform transf) {
    int result = Parse(state, rle);

    if (result == SUCCESS)
      state.Transform(dx, dy, transf);

    return result;
  }

  static LifeState Parse(const char *rle, int dx, int dy,
                         SymmetryTransform trans) {
    LifeState result;
    Parse(result, rle);
    result.Transform(dx, dy, trans);

    return result;
  }

  static LifeState Parse(const char *rle, int dx, int dy) {
    LifeState result;
    Parse(result, rle, dx, dy);

    return result;
  }

  static LifeState Parse(const char *rle) { return Parse(rle, 0, 0); }

  std::string RLE() const;

  static LifeState RandomState() {
    LifeState result;
    for (int i = 0; i < N; i++)
      result[i] = PRNG::dist(PRNG::e2);

    return result;
  }

#ifdef __AVX2__
  // https://stackoverflow.com/questions/56153183/is-using-avx2-can-implement-a-faster-processing-of-lzcnt-on-a-word-array
  std::pair<int, int> FirstOn() const
  {
    const char *p = (const char *)state;
    size_t len = 8*N;
    //assert(len % 64 == 0);
    //optimal if p is 64-byte aligned, so we're checking single cache-lines
    const char *p_init = p;
    const char *endp = p + len;
    do {
      __m256i v1 = _mm256_loadu_si256((const __m256i*)p);
      __m256i v2 = _mm256_loadu_si256((const __m256i*)(p+32));
      __m256i vor = _mm256_or_si256(v1,v2);
      if (!_mm256_testz_si256(vor, vor)) {        // find the first non-zero cache line
        __m256i v1z = _mm256_cmpeq_epi32(v1, _mm256_setzero_si256());
        __m256i v2z = _mm256_cmpeq_epi32(v2, _mm256_setzero_si256());
        uint32_t zero_map = _mm256_movemask_ps(_mm256_castsi256_ps(v1z));
        zero_map |= _mm256_movemask_ps(_mm256_castsi256_ps(v2z)) << 8;

        unsigned idx = __builtin_ctz(~zero_map);  // Use ctzll for GCC, because GCC is dumb and won't optimize away a movsx
        uint32_t nonzero_chunk;
        memcpy(&nonzero_chunk, p+4*idx, sizeof(nonzero_chunk));  // aliasing / alignment-safe load
        if(idx % 2 == 0) {
          return std::make_pair((p-p_init + 4*idx)/8, __builtin_ctz(nonzero_chunk));
        } else {
          return std::make_pair((p-p_init + 4*(idx-1))/8, __builtin_ctz(nonzero_chunk) + 32);
        }
      }
      p += 64;
    } while(p < endp);
    return std::make_pair(-1, -1);
  }
#else
  std::pair<int, int> FirstOn() const {
    int foundq = N;
    for (int x = 0; x < N; x+=4) {
      if (state[x] != 0ULL ||
          state[x+1] != 0ULL ||
          state[x+2] != 0ULL ||
          state[x+3] != 0ULL) {
        foundq = x;
      }
    }
    if (foundq == N) {
      return std::make_pair(-1, -1);
    }

    int foundx;
    if (state[foundq] != 0ULL) {
      foundx = foundq;
    } else if (state[foundq + 1] != 0ULL) {
      foundx = foundq + 1;
    } else if (state[foundq + 2] != 0ULL) {
      foundx = foundq + 2;
    } else if (state[foundq + 3] != 0ULL) {
      foundx = foundq + 3;
    }
    return std::make_pair(foundx, __builtin_ctzll(state[foundx]));
  }
#endif

  LifeState FirstCell() const {
    std::pair<int, int> pair = FirstOn();
    LifeState result;
    result.Set(pair.first, pair.second);
    return result;
  }

  static LifeState SolidRect(int x, int y, int w, int h) {
    uint64_t column;
    if (h < 64)
      column = RotateLeft(((uint64_t)1 << h) - 1, y);
    else
      column = ~0ULL;

    unsigned start, end;
    if (w < N) {
      start = (x + N) % N;
      end = (x + w + N) % N;
    } else {
      start = 0;
      end = N;
    }

    LifeState result;
    if (end > start) {
      for (unsigned int i = start; i < end; i++)
        result[i] = column;
    } else {
      for (unsigned int i = 0; i < end; i++)
        result[i] = column;
      for (unsigned int i = start; i < N; i++)
        result[i] = column;
    }
    return result;
  }

  std::array<int, 4> XYBounds() const {
    int minCol = -(N/2);
    int maxCol = (N/2)-1;

    for (int i = -(N/2); i <= (N/2)-1; i++) {
      if (state[(i + N) % N] != 0) {
        minCol = i;
        break;
      }
    }

    for (int i = (N/2)-1; i >= -(N/2); i--) {
      if (state[(i + N) % N] != 0) {
        maxCol = i;
        break;
      }
    }

    uint64_t orOfCols(0);
    for (int i = minCol; i <= maxCol; ++i) {
      orOfCols = orOfCols | state[(i + N) % N];
    }
    if (orOfCols == 0ULL) {
      return std::array<int, 4>({0, 0, 0, 0});
    }
    orOfCols = __builtin_rotateright64(orOfCols, 32);
    int topMargin = __builtin_ctzll(orOfCols);
    int bottomMargin = __builtin_clzll(orOfCols);
    return std::array<int, 4>(
        {minCol, topMargin - 32, maxCol, 31 - bottomMargin});
  }

#if N > 64
#error "PopulatedColumns cannot handle N > 64"
#endif

  uint64_t PopulatedColumns() const {
    uint64_t result = 0;
    for (unsigned i = 0; i < N; i++)
      if(state[i] != 0)
        result |= 1 << i;
    return result;
  }

  std::pair<int,int> WidthHeight() const {
    uint64_t orOfCols = 0;
    for (unsigned i = 0; i < N; ++i)
      orOfCols |= state[i];

    if (orOfCols == 0ULL) // empty grid.
      return std::make_pair(0,0);


    uint64_t cols = PopulatedColumns();
#if N == 64
    unsigned width = populated_width_uint64_t(cols);
#elif N == 32
    unsigned width = populated_width_uint32_t((uint32_t)cols);
#else
#error "WidthHeight cannot handle N"
#endif

    unsigned height = populated_width_uint64_t(orOfCols);

    return {width, height};
  }

  LifeState ComponentContaining(const LifeState &seed, const LifeState &corona) const {
    LifeState result;
    LifeState tocheck = seed;
    while (!tocheck.IsEmpty()) {
      LifeState neighbours = tocheck.Convolve(corona) & *this;
      tocheck = neighbours & ~result;
      result |= neighbours;
    }

    return result;
  }

  LifeState ComponentContaining(const LifeState &seed) const {
    LifeState corona = LifeState::Parse("b3o$5o$5o$5o$b3o!");
    corona.Move(-2, -2);
    return ComponentContaining(seed, corona);
  }

  std::vector<LifeState> Components() const {
    std::vector<LifeState> result;
    LifeState remaining = *this;
    while (!remaining.IsEmpty()) {
      LifeState component = remaining.ComponentContaining(remaining.FirstCell());
      result.push_back(component);
      remaining &= ~component;
    }
    return result;
  }
};

void LifeState::Step() {
  uint64_t tempxor[N];
  uint64_t tempand[N];

  for (int i = 0; i < N; i++) {
    uint64_t l = RotateLeft(state[i]);
    uint64_t r = RotateRight(state[i]);
    tempxor[i] = l ^ r ^ state[i];
    tempand[i] = ((l ^ r) & state[i]) | (l & r);
  }

  #pragma clang loop unroll(full)
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

    state[i] = Rokicki(state[i], tempxor[idxU], tempand[idxU], tempxor[idxB], tempand[idxB]);
  }

  // int s = min + 1;
  // int e = max - 1;

  // if (s == 1)
  //   s = 0;

  // if (e == N - 2)
  //   e = N - 1;

  // for (int i = s; i <= e; i++) {
  //   state[i] = tempState[i];
  // }
  //
}

void LifeState::Transform(SymmetryTransform transf) {
  switch (transf) {
  case Identity:
    break;
  case ReflectAcrossXEven:
    FlipX();
    break;
  case ReflectAcrossX:
    FlipX();
    Move(0, 1);
    break;
  case ReflectAcrossYEven:
    FlipY();
    break;
  case ReflectAcrossY:
    FlipY();
    Move(1, 0);
    break;
  case Rotate180EvenBoth:
    FlipX();
    FlipY();
    break;
  case Rotate180EvenVertical:
    FlipX();
    FlipY();
    Move(1, 0);
    break;
  case Rotate180EvenHorizontal:
    FlipX();
    FlipY();
    Move(0, 1);
    break;
  case Rotate180OddBoth:
    FlipX();
    FlipY();
    Move(1, 1);
    break;
  case ReflectAcrossYeqX:
    Transpose(false);
    break;
  case ReflectAcrossYeqNegX:
    Transpose(true);
    break;
  case ReflectAcrossYeqNegXP1:
    Transpose(true);
    Move(1, 1);
    break;
  case Rotate90Even:
    FlipX();
    Transpose(false);
    break;
  case Rotate90:
    FlipX();
    Transpose(false);
    Move(1, 0);
    break;
  case Rotate270Even:
    FlipY();
    Transpose(false);
    break;
  case Rotate270:
    FlipY();
    Transpose(false);
    Move(0, 1);
    break;
  }
}

void LifeState::Print() const {
  for (int j = 0; j < 64; j++) {
    for (int i = 0; i < N; i++) {
      if (GetCell(i - (N/2), j - 32) == 0) {
        int hor = 0;
        int ver = 0;

        if ((j - 32) % 10 == 0)
          hor = 1;

        if ((i - (N/2)) % 10 == 0)
          ver = 1;

        if (hor == 1 && ver == 1)
          printf("+");
        else if (hor == 1)
          printf("-");
        else if (ver == 1)
          printf("|");
        else
          printf(".");
      } else
        printf("O");
    }
    printf("\n");
  }

  printf("\n\n\n\n\n\n");
}

int LifeState::Parse(LifeState &state, const char *rle, int starti) {
  char ch;
  int cnt, i, j;
  int x, y;
  x = 0;
  y = 0;
  cnt = 0;

  i = starti;

  while ((ch = rle[i]) != '\0') {

    if (ch >= '0' && ch <= '9') {
      cnt *= 10;
      cnt += (ch - '0');
    } else if (ch == 'o') {

      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        state.SetCell(x, y, 1);
        x++;
      }

      cnt = 0;
    } else if (ch == 'b') {
      if (cnt == 0)
        cnt = 1;

      x += cnt;
      cnt = 0;

    } else if (ch == '$') {
      if (cnt == 0)
        cnt = 1;

      if (cnt == 129)
        return i + 1;

      y += cnt;
      x = 0;
      cnt = 0;
    } else if (ch == '!') {
      break;
    } else {
      return -2;
    }

    i++;
  }

  return -1;
}

std::string LifeState::RLE() const {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < 64; j++) {
    bool last_val = GetCell(0 - (N/2), j - 32) == 1;
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      bool val = GetCell(i - (N/2), j - 32) == 1;

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

std::pair<int, int> LifeState::FindSetNeighbour(std::pair<int, int> cell) const {
  // This could obviously be done faster by extracting the result
  // directly from the columns, but this is probably good enough for now
  const std::array<std::pair<int, int>, 9> directions = {std::make_pair(0,0), {-1, 0}, {1, 0}, {0,1}, {0, -1}, {-1,-1}, {-1,1}, {1, -1}, {1, 1}};
  for (auto d : directions) {
    int x = (cell.first + d.first + N) % N;
    int y = (cell.second + d.second + 64) % 64;
    if (Get(x, y))
      return std::make_pair(x, y);
  }
  return std::make_pair(-1, -1);
}

unsigned LifeState::NeighbourhoodCount(std::pair<int, int> cell) const {
  unsigned result = 0;
  // ditto
  const std::array<std::pair<int, int>, 9> directions = {std::make_pair(-1,-1), {-1,0}, {-1,1}, {0,-1}, {0, 0}, {0,1}, {1, -1}, {1, 0}, {1, 1}};
  for (auto d : directions) {
    int x = (cell.first + d.first + N) % N;
    int y = (cell.second + d.second + 64) % 64;
    if (Get(x, y))
      result++;
  }
  return result;
}

class LifeTarget {
public:
  LifeState wanted;
  LifeState unwanted;

  LifeTarget() {}
  LifeTarget(LifeState &state) {
    wanted = state;
    unwanted = state.GetBoundary();
  }

  void Transform(SymmetryTransform transf) {
    wanted.Transform(transf);
    unwanted.Transform(transf);
  }

  static int Parse(LifeTarget &target, const char *rle, int x, int y,
                   SymmetryTransform transf) {
    LifeState Temp;
    int result = LifeState::Parse(Temp, rle, x, y, transf);

    if (result == SUCCESS) {
      target.wanted = Temp;
      target.unwanted = Temp.GetBoundary();
      return SUCCESS;
    }

    return FAIL;
  }

  static LifeTarget Parse(const char *rle, int x, int y,
                          SymmetryTransform transf) {
    LifeTarget target;
    Parse(target, rle, x, y, transf);
    return target;
  }

  static LifeTarget Parse(const char *rle, int x, int y) {
    return Parse(rle, x, y, Identity);
  }

  static LifeTarget Parse(const char *rle) { return Parse(rle, 0, 0); }
};

inline bool LifeState::Contains(const LifeTarget &target, int dx,
                                int dy) const {
  return Contains(target.wanted, dx, dy) &&
         AreDisjoint(target.unwanted, dx, dy);
}

inline bool LifeState::Contains(const LifeTarget &target) const {
  return Contains(target.wanted) && AreDisjoint(target.unwanted);
}

inline LifeState LifeState::Match(const LifeTarget &target) const {
  return MatchLiveAndDead(target.wanted, target.unwanted);
}

std::vector<std::pair<int, int>> LifeState::OnCells() const {
  LifeState remaining = *this;
  std::vector<std::pair<int, int>> result;
  for(int pop = remaining.GetPop(); pop > 0; pop--) {
    auto cell = remaining.FirstOn();
    result.push_back(cell);
    remaining.Erase(cell.first, cell.second);
  }
  return result;
}
