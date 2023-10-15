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
#include <array>
#include <vector>
#include <iostream>
#include <sstream>
#include <random>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Best if multiple of 4
#define N 64
// #define N 32

#define SUCCESS 1
#define FAIL 0

// GCC
#ifdef __GNUC__
#ifndef __clang__
#include <x86intrin.h>

constexpr uint64_t reverse_uint64_t(uint64_t x) {
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

constexpr int __builtin_ctzll(uint64_t x) {
  unsigned long log2;
  _BitScanReverse64(&log2, x);
  return log2;
}
#endif

constexpr unsigned longest_run_uint64_t(uint64_t x) {
  if(x == 0)
    return 0;

  if(x == ~0ULL)
    return 64;

  std::array<uint64_t, 6> pow2runs = {0};
  for (unsigned n = 0; n < 6; n++) {
    pow2runs[n] = x;
    x &= __builtin_rotateleft64(x, 1 << n);
  }

  unsigned last = 5;
  for (unsigned n = 0; n < 6; n++) {
    if (pow2runs[n] == 0) {
      last = n-1;
      break;
    }
  }

  x = pow2runs[last];
  unsigned count = 1 << last;

  for (int n = 5; n >= 0; n--) {
    uint64_t y = (x & __builtin_rotateleft64(x, 1 << n));
    if (y != 0 && n < last) {
      count += 1 << n;
      x = y;
    }
  }

  return count;
}

constexpr unsigned populated_width_uint64_t(uint64_t x) {
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

constexpr unsigned longest_run_uint32_t(uint32_t x) {
  if(x == 0)
    return 0;

  if(x == ~0U)
    return 32;

  std::array<uint32_t, 5> pow2runs = {0};
  for (unsigned n = 0; n < 5; n++) {
    pow2runs[n] = x;
    x &= __builtin_rotateleft64(x, 1 << n);
  }

  unsigned last = 4;
  for (unsigned n = 0; n < 5; n++) {
    if (pow2runs[n] == 0) {
      last = n-1;
      break;
    }
  }

  x = pow2runs[last];
  unsigned count = 1 << last;

  for (int n = 4; n >= 0; n--) {
    uint64_t y = (x & __builtin_rotateleft64(x, 1 << n));
    if (y != 0 && n < last) {
      count += 1 << n;
      x = y;
    }
  }

  return count;
}

constexpr unsigned populated_width_uint32_t(uint32_t x) {
  if (x == 0)
    return 0;

  // First, shift to try and make it 2^n-1
  int lzeroes = __builtin_ctz(x);
  x = __builtin_rotateright32(x, lzeroes);
  int tones = __builtin_clz(~x);
  x = __builtin_rotateleft32(x, tones);

  if ((x & (x + 1)) == 0)
    return __builtin_ctz(~x);

  return 32 - longest_run_uint32_t(~x);
}

constexpr uint64_t convolve_uint64_t(uint64_t x, uint64_t y) {
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

// Taken from https://github.com/wangyi-fudan/wyhash
namespace HASH {
  static inline void _wymum(uint64_t *A, uint64_t *B){
#if defined(__SIZEOF_INT128__)
    __uint128_t r=*A; r*=*B;
    *A=(uint64_t)r; *B=(uint64_t)(r>>64);
#elif defined(_MSC_VER) && defined(_M_X64)
    *A=_umul128(*A,*B,B);
#else
    uint64_t ha=*A>>32, hb=*B>>32, la=(uint32_t)*A, lb=(uint32_t)*B, hi, lo;
    uint64_t rh=ha*hb, rm0=ha*lb, rm1=hb*la, rl=la*lb, t=rl+(rm0<<32), c=t<rl;
    lo=t+(rm1<<32); c+=lo<t; hi=rh+(rm0>>32)+(rm1>>32)+c;
    *A=lo;  *B=hi;
#endif
  }

  static inline uint64_t _wymix(uint64_t A, uint64_t B){
    _wymum(&A,&B);
    return A^B;
  }

  static inline uint64_t hash64(uint64_t A, uint64_t B){
    A ^= 0xa0761d6478bd642full;
    B ^= 0xe7037ed1a0b428dbull;
    _wymum(&A,&B);
    return _wymix(A^0xa0761d6478bd642full, B^0xe7037ed1a0b428dbull);
  }
} // namespace HASH

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

constexpr uint64_t RotateLeft(uint64_t x, unsigned int k) {
  return __builtin_rotateleft64(x, k);
}

constexpr uint64_t RotateRight(uint64_t x, unsigned int k) {
  return __builtin_rotateright64(x, k);
}

constexpr uint64_t RotateLeft(uint64_t x) { return RotateLeft(x, 1); }
constexpr uint64_t RotateRight(uint64_t x) { return RotateRight(x, 1); }

class LifeTarget;

class LifeState {
public:
  uint64_t state[N];

  constexpr LifeState() : state{0} {}
  LifeState(__attribute__((unused)) bool dummy) {}

  void Set(unsigned x, unsigned y) { state[x] |= (1ULL << y); }
  void Erase(unsigned x, unsigned y) { state[x] &= ~(1ULL << y); }
  bool Get(unsigned x, unsigned y) const { return (state[x] & (1ULL << y)) != 0; }
  void Set(unsigned x, unsigned y, bool val) {
    if(val)
      Set(x, y);
    else
      Erase(x, y);
  }

  void SetSafe(int x, int y, bool val) {
    if (val)
      Set((x + N) % N, (y + 64) % 64);
    else
      Erase((x + N) % N, (y + 64) % 64);
  }
  bool GetSafe(int x, int y) const {
    return Get((x + N) % N, (y + 64) % 64);
  }

  void Set(std::pair<int, int> cell) { Set(cell.first, cell.second); };
  void Erase(std::pair<int, int> cell) { Erase(cell.first, cell.second); };
  int Get(std::pair<int, int> cell) const { return Get(cell.first, cell.second); };
  void SetSafe(std::pair<int, int> cell, bool val) { SetSafe(cell.first, cell.second, val); };
  void Set(std::pair<int, int> cell, bool val) { Set(cell.first, cell.second, val); };
  int GetSafe(std::pair<int, int> cell) const { return GetSafe(cell.first, cell.second); };

  constexpr uint64_t& operator[](unsigned i) { return state[i]; }
  constexpr const uint64_t& operator[](unsigned i) const { return state[i]; }

  uint64_t GetHash() const {
    uint64_t result = 0;

    for (int i = 0; i < N; i++) {
      result = HASH::hash64(result, state[i]);
    }

    return result;
  }

  uint64_t GetOctoHash() const {
    uint64_t result = 0;

    auto allTransforms = {
        Identity,           ReflectAcrossXEven,   ReflectAcrossYeqX,
        ReflectAcrossYEven, ReflectAcrossYeqNegX, Rotate90Even,
        Rotate270Even,      Rotate180EvenBoth};

    for (auto t : allTransforms) {
      LifeState transformed = *this;
      transformed.Transform(t);
      auto [x, y, _x2, _y2] = transformed.XYBounds();
      transformed.Move(-x, -y);
      result ^= transformed.GetHash();
    }

    return result;
  }

public:
  void Print() const;

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
      transformed |= soFar;
    }
    *this |= transformed;
  }

  void JoinWSymChain(const LifeState &state,
                     const std::vector<SymmetryTransform> &symChain) {
    LifeState transformed = state;

    for (auto sym : symChain) {
      LifeState soFar = transformed;
      soFar.Transform(sym);
      transformed |= soFar;
    }
    *this |= transformed;
  }

  unsigned GetPop() const {
    unsigned pop = 0;

    for (int i = 0; i < N; i++) {
      pop += __builtin_popcountll(state[i]);
    }

    return pop;
  }

  std::vector<std::pair<int, int>> OnCells() const;

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

  constexpr LifeState Moved(int x, int y) {
    LifeState result;

    if (x < 0)
      x += N;
    if (y < 0)
      y += 64;

    for (int i = 0; i < N; i++) {
      int newi = (i + x) % 64;
      result[newi] = RotateLeft(state[i], y);
    }
    return result;
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

    for (j = N/2, m = (~0ULL) >> (N/2); j; j >>= 1, m ^= m << j) {
      for (k = 0; k < N; k = ((k | j) + 1) & ~j) {
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
    return ZOI() & ~*this;
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

  std::pair<int, int> FindSetNeighbour(std::pair<int, int> cell) const {
    // This could obviously be done faster by extracting the result
    // directly from the columns, but this is probably good enough for now
    const std::array<std::pair<int, int>, 9> directions = {std::make_pair(0, 0), {-1, 0}, {1, 0}, {0,1}, {0, -1}, {-1,-1}, {-1,1}, {1, -1}, {1, 1}};
    for (auto d : directions) {
      int x = (cell.first + d.first + N) % N;
      int y = (cell.second + d.second + 64) % 64;
      if (Get(x, y))
        return std::make_pair(x, y);
    }
    return std::make_pair(-1, -1);
  }

  unsigned CountNeighboursWithCenter(std::pair<int, int> cell) const {
    unsigned result = 0;
    for (int i = -1; i <= 1; i++) {
      uint64_t column = state[(cell.first + i + N) % N];
      column = RotateRight(column, (cell.second - 1 + 64) % 64);
      result += __builtin_popcountll(column & 0b111);
    }
    return result;
  }

  unsigned CountNeighbours(std::pair<int, int> cell) const {
    return CountNeighboursWithCenter(cell) - (Get(cell) ? 1 : 0);
  }

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
    }
  }

  static LifeState Parse(const char *rle);

  static LifeState Parse(const char *rle, int dx, int dy) {
    LifeState result = LifeState::Parse(rle);
    result.Move(dx, dy);
    return result;
  }

  consteval static LifeState ConstantParse(const char *rle) {
    LifeState result;

    char ch = 0;
    int cnt = 0;
    int x = 0;
    int y = 0;
    int i = 0;

    while ((ch = rle[i]) != '\0') {
      if (ch >= '0' && ch <= '9') {
        cnt *= 10;
        cnt += (ch - '0');
      } else if (ch == 'o') {
        if (cnt == 0)
          cnt = 1;

        for (int j = 0; j < cnt; j++) {
          result.state[x] |= (1ULL << (y));
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
          return LifeState();

        y += cnt;
        x = 0;
        cnt = 0;
      } else if (ch == '!') {
        break;
      } else {
        return LifeState();
      }

      i++;
    }

    return result;
  }

  consteval static LifeState ConstantParse(const char *rle, int dx, int dy) {
    return LifeState::ConstantParse(rle).Moved(dx, dy);
  }

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
    unsigned foundq = N;
    for (int x = 0; x < N; x += 4) {
      if ((state[x] | state[x + 1] | state[x + 2] | state[x + 3]) != 0ULL) {
        foundq = x;
      }
    }
    if (foundq == N) {
      return std::make_pair(-1, -1);
    }

    if (state[foundq] != 0ULL) {
      return std::make_pair(foundq, __builtin_ctzll(state[foundq]));
    } else if (state[foundq + 1] != 0ULL) {
      return std::make_pair(foundq + 1, __builtin_ctzll(state[foundq + 1]));
    } else if (state[foundq + 2] != 0ULL) {
      return std::make_pair(foundq + 2, __builtin_ctzll(state[foundq + 2]));
    } else if (state[foundq + 3] != 0ULL) {
      return std::make_pair(foundq + 3, __builtin_ctzll(state[foundq + 3]));
    }
  }
#endif

  LifeState FirstCell() const {
    std::pair<int, int> pair = FirstOn();
    LifeState result;
    result.Set(pair.first, pair.second);
    return result;
  }

  static LifeState Cell(std::pair<int, int> cell) {
    LifeState result;
    result.Set(cell.first, cell.second);
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

  static LifeState SolidRectXY(int x1, int y1, int x2, int y2) {
    return SolidRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
  }

  static LifeState NZOIAround(std::pair<int, int> cell, unsigned distance) {
    unsigned size = 2 * distance + 1;
    return LifeState::SolidRect(cell.first - distance, cell.second - distance,
                                size, size);
  }

  static LifeState CellZOI(std::pair<int, int> cell) {
    return LifeState::NZOIAround(cell, 1);
  }

  LifeState NZOI(unsigned distance) {
    return Convolve(LifeState::NZOIAround({0, 0}, distance));
  }

  std::array<int, 4> XYBounds() const {
#if N == 64
    uint64_t popCols = PopulatedColumns();
    popCols = __builtin_rotateright64(popCols, 32);
    int leftMargin  = __builtin_ctzll(popCols);
    int rightMargin = __builtin_clzll(popCols);
#elif N == 32
    uint32_t popCols = PopulatedColumns();
    popCols = __builtin_rotateright32(popCols, 16);
    int leftMargin  = __builtin_ctz(popCols);
    int rightMargin = __builtin_clz(popCols);
#else
#error "XYBounds cannot handle N = " N
#endif

    uint64_t orOfCols = 0;
    for (unsigned i = 0; i < N; ++i)
      orOfCols |= state[i];

    if (orOfCols == 0ULL) {
      return std::array<int, 4>({-1, -1, -1, -1});
    }

    orOfCols = __builtin_rotateright64(orOfCols, 32);
    int topMargin = __builtin_ctzll(orOfCols);
    int bottomMargin = __builtin_clzll(orOfCols);

#if N == 64
    return std::array<int, 4>(
        {leftMargin - 32, topMargin - 32, 31 - rightMargin, 31 - bottomMargin});
#elif N == 32
    return std::array<int, 4>(
        {leftMargin - 16, topMargin - 32, 15 - rightMargin, 31 - bottomMargin});
#endif

  }

#if N > 64
#error "PopulatedColumns cannot handle N > 64"
#endif

  uint64_t PopulatedColumns() const {
    uint64_t result = 0;
    for (unsigned i = 0; i < N; i++)
      if(state[i] != 0)
        result |= 1ULL << i;
    return result;
  }

  std::pair<int,int> WidthHeight() const {
    uint64_t orOfCols = 0;
    for (unsigned i = 0; i < N; ++i)
      orOfCols |= state[i];

    if (orOfCols == 0ULL) // empty grid.
      return std::make_pair(0, 0);


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

  LifeState BufferAround(std::pair<int, int> size) const {
    auto bounds = XYBounds();

    if(bounds[0] == -1)
      return ~LifeState();

    int width = bounds[2] - bounds[0] + 1;
    int height = bounds[3] - bounds[1] + 1;

    int remainingwidth = size.first - width;
    int remainingheight = size.second - height;

    if (remainingwidth < 0 || remainingheight < 0)
      return LifeState();
    else
      return LifeState::SolidRectXY(bounds[0] - remainingwidth,
                                    bounds[1] - remainingheight,
                                    bounds[2] + remainingwidth,
                                    bounds[3] + remainingheight);
  }

  std::pair<int, int> CenterPoint() {
    auto bounds = XYBounds();
    auto w = bounds[2] - bounds[0];
    auto h = bounds[3] - bounds[1];
    return {bounds[0] + w/2, bounds[1] + h/2};
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
    constexpr LifeState corona = LifeState::ConstantParse("b3o$5o$5o$5o$b3o!", -2, -2);
    return ComponentContaining(seed, corona);
  }

  std::vector<LifeState> Components(const LifeState &corona) const {
    std::vector<LifeState> result;
    LifeState remaining = *this;
    while (!remaining.IsEmpty()) {
      LifeState component = remaining.ComponentContaining(remaining.FirstCell(), corona);
      result.push_back(component);
      remaining &= ~component;
    }
    return result;
  }
  std::vector<LifeState> Components() const {
    constexpr LifeState corona = LifeState::ConstantParse("b3o$5o$5o$5o$b3o!", -2, -2);
    return Components(corona);
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
      if (GetSafe(i - (N/2), j - 32) == 0) {
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

LifeState LifeState::Parse(const char *rle) {
  LifeState result;

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
    } else if (ch == 'o') {

      if (cnt == 0)
        cnt = 1;

      for (j = 0; j < cnt; j++) {
        result.SetSafe(x, y, 1);
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
        return LifeState();

      y += cnt;
      x = 0;
      cnt = 0;
    } else if (ch == '!') {
      break;
    } else {
      return LifeState();
    }

    i++;
  }

  return result;
}

std::string LifeState::RLE() const {
  std::stringstream result;

  unsigned eol_count = 0;

  for (unsigned j = 0; j < 64; j++) {
    bool last_val = GetSafe(0 - (N/2), j - 32) == 1;
    unsigned run_count = 0;

    for (unsigned i = 0; i < N; i++) {
      bool val = GetSafe(i - (N/2), j - 32) == 1;

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
