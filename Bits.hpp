#pragma once

#include <stdint.h>

#include "LifeAPI.h"

inline void HalfAdd(uint64_t &outbit, uint64_t &outcarry, const uint64_t ina, const uint64_t inb) {
  outbit = ina ^ inb;
  outcarry = ina & inb;
}

inline void FullAdd(uint64_t &outbit, uint64_t &outcarry, const uint64_t ina, const uint64_t inb, const uint64_t inc) {
  uint64_t halftotal = ina ^ inb;
  outbit = halftotal ^ inc;
  uint64_t halfcarry1 = ina & inb;
  uint64_t halfcarry2 = inc & halftotal;
  outcarry = halfcarry1 | halfcarry2;
}

inline void HalfAdd(LifeState &__restrict__ outbit, LifeState &__restrict__ outcarry, const LifeState &__restrict__ ina, const LifeState &__restrict__ inb) {
  outbit = ina ^ inb;
  outcarry = ina & inb;
}

inline void FullAdd(LifeState &__restrict__ outbit, LifeState &__restrict__ outcarry, const LifeState &__restrict__ ina, const LifeState &__restrict__ inb, const LifeState &__restrict__ inc) {
  LifeState halftotal = ina ^ inb;
  outbit = halftotal ^ inc;
  LifeState halfcarry1 = ina & inb;
  LifeState halfcarry2 = inc & halftotal;
  outcarry = halfcarry1 | halfcarry2;
}

inline void FourBitAdd(const LifeState &__restrict__ a3,
                       const LifeState &__restrict__ a2,
                       const LifeState &__restrict__ a1,
                       const LifeState &__restrict__ a0,
                       const LifeState &__restrict__ b3,
                       const LifeState &__restrict__ b2,
                       const LifeState &__restrict__ b1,
                       const LifeState &__restrict__ b0,
                       const LifeState &__restrict__ incarry,
                       LifeState &__restrict__ result3,
                       LifeState &__restrict__ result2,
                       LifeState &__restrict__ result1,
                       LifeState &__restrict__ result0) {
  LifeState carry = incarry;
  FullAdd(result0, carry, a0, b0, carry);
  FullAdd(result1, carry, a1, b1, carry);
  FullAdd(result2, carry, a2, b2, carry);
  FullAdd(result3, carry, a3, b3, carry);
}

inline void FourBitAdd(const LifeState &__restrict__ a3,
                       const LifeState &__restrict__ a2,
                       const LifeState &__restrict__ a1,
                       const LifeState &__restrict__ a0,
                       const LifeState &__restrict__ b3,
                       const LifeState &__restrict__ b2,
                       const LifeState &__restrict__ b1,
                       const LifeState &__restrict__ b0,
                       LifeState &__restrict__ result3,
                       LifeState &__restrict__ result2,
                       LifeState &__restrict__ result1,
                       LifeState &__restrict__ result0) {
  FourBitAdd(a3, a2, a1, a0, b3, b2, b1, b0, LifeState(), result3, result2, result1, result0);
}

// Two's complement
inline void FourBitSubtract(const LifeState &__restrict__ a3,
                       const LifeState &__restrict__ a2,
                       const LifeState &__restrict__ a1,
                       const LifeState &__restrict__ a0,
                       const LifeState &__restrict__ b3,
                       const LifeState &__restrict__ b2,
                       const LifeState &__restrict__ b1,
                       const LifeState &__restrict__ b0,
                       LifeState &__restrict__ result3,
                       LifeState &__restrict__ result2,
                       LifeState &__restrict__ result1,
                       LifeState &__restrict__ result0) {
  FourBitAdd(a3, a2, a1, a0, ~b3, ~b2, ~b1, ~b0, ~LifeState(), result3, result2, result1, result0);
}

inline void FullAdd(std::array<uint64_t, 4> &__restrict__ outbit,
                    std::array<uint64_t, 4> &__restrict__ outcarry,
                    const std::array<uint64_t, 4> &__restrict__ ina,
                    const std::array<uint64_t, 4> &__restrict__ inb,
                    const std::array<uint64_t, 4> &__restrict__ inc) {
  for(unsigned i = 0; i < 4; i++) {
    uint64_t halftotal = ina[i] ^ inb[i];
    outbit[i] = halftotal ^ inc[i];
    uint64_t halfcarry1 = ina[i] & inb[i];
    uint64_t halfcarry2 = inc[i] & halftotal;
    outcarry[i] = halfcarry1 | halfcarry2;
  }
}

inline void FourBitAdd(const std::array<uint64_t, 4> &__restrict__ a3,
                       const std::array<uint64_t, 4> &__restrict__ a2,
                       const std::array<uint64_t, 4> &__restrict__ a1,
                       const std::array<uint64_t, 4> &__restrict__ a0,
                       const std::array<uint64_t, 4> &__restrict__ b3,
                       const std::array<uint64_t, 4> &__restrict__ b2,
                       const std::array<uint64_t, 4> &__restrict__ b1,
                       const std::array<uint64_t, 4> &__restrict__ b0,
                       const std::array<uint64_t, 4> &__restrict__ incarry,
                       std::array<uint64_t, 4> &__restrict__ result3,
                       std::array<uint64_t, 4> &__restrict__ result2,
                       std::array<uint64_t, 4> &__restrict__ result1,
                       std::array<uint64_t, 4> &__restrict__ result0) {
  std::array<uint64_t, 4> carry = incarry;
  FullAdd(result0, carry, a0, b0, carry);
  FullAdd(result1, carry, a1, b1, carry);
  FullAdd(result2, carry, a2, b2, carry);
  FullAdd(result3, carry, a3, b3, carry);
}

inline void FourBitAdd(const std::array<uint64_t, 4> &__restrict__ a3,
                       const std::array<uint64_t, 4> &__restrict__ a2,
                       const std::array<uint64_t, 4> &__restrict__ a1,
                       const std::array<uint64_t, 4> &__restrict__ a0,
                       const std::array<uint64_t, 4> &__restrict__ b3,
                       const std::array<uint64_t, 4> &__restrict__ b2,
                       const std::array<uint64_t, 4> &__restrict__ b1,
                       const std::array<uint64_t, 4> &__restrict__ b0,
                       std::array<uint64_t, 4> &__restrict__ result3,
                       std::array<uint64_t, 4> &__restrict__ result2,
                       std::array<uint64_t, 4> &__restrict__ result1,
                       std::array<uint64_t, 4> &__restrict__ result0) {
  FourBitAdd(a3, a2, a1, a0, b3, b2, b1, b0, {0, 0, 0, 0}, result3, result2, result1, result0);
}

inline void FourBitSubtract(const std::array<uint64_t, 4> &__restrict__ a3,
                       const std::array<uint64_t, 4> &__restrict__ a2,
                       const std::array<uint64_t, 4> &__restrict__ a1,
                       const std::array<uint64_t, 4> &__restrict__ a0,
                       const std::array<uint64_t, 4> &__restrict__ b3,
                       const std::array<uint64_t, 4> &__restrict__ b2,
                       const std::array<uint64_t, 4> &__restrict__ b1,
                       const std::array<uint64_t, 4> &__restrict__ b0,
                       std::array<uint64_t, 4> &__restrict__ result3,
                       std::array<uint64_t, 4> &__restrict__ result2,
                       std::array<uint64_t, 4> &__restrict__ result1,
                       std::array<uint64_t, 4> &__restrict__ result0) {
  FourBitAdd(a3, a2, a1, a0,
             {~b3[0], ~b3[1], ~b3[2], ~b3[3]},
             {~b2[0], ~b2[1], ~b2[2], ~b2[3]},
             {~b1[0], ~b1[1], ~b1[2], ~b1[3]},
             {~b0[0], ~b0[1], ~b0[2], ~b0[3]},
             {~0ULL, ~0ULL, ~0ULL, ~0ULL}, result3, result2, result1, result0);
}

void CountRows(const LifeState &__restrict__ state, uint64_t (&__restrict__ col0)[N + 2], uint64_t (&__restrict__ col1)[N + 2]) {
  for (int i = 0; i < N; i++) {
    uint64_t a = state.state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    col0[i+1] = l ^ r ^ a;
    col1[i+1] = ((l ^ r) & a) | (l & r);
  }
}

void CountNeighbourhood(const LifeState &__restrict__ state,
                              LifeState &__restrict__ bit3,
                              LifeState &__restrict__ bit2,
                              LifeState &__restrict__ bit1,
                              LifeState &__restrict__ bit0) {
  uint64_t col0[N + 2];
  uint64_t col1[N + 2];
  CountRows(state, col0, col1);

  col0[0] = col0[N]; col0[N+1] = col0[1];
  col1[0] = col1[N]; col1[N+1] = col1[1];
  for (int i = 0; i < N; i++) {
    uint64_t u_on0 = col0[i];
    uint64_t c_on0 = col0[i+1];
    uint64_t l_on0 = col0[i+2];
    uint64_t u_on1 = col1[i];
    uint64_t c_on1 = col1[i+1];
    uint64_t l_on1 = col1[i+2];

    uint64_t on3, on2, on1, on0;

    uint64_t uc0, uc1, uc2, uc_carry0;
    HalfAdd(uc0, uc_carry0, u_on0, c_on0);
    FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

    uint64_t on_carry1, on_carry0;
    HalfAdd(on0, on_carry0, uc0, l_on0);
    FullAdd(on1, on_carry1, uc1, l_on1, on_carry0);
    HalfAdd(on2, on3, uc2, on_carry1);

    bit3.state[i] = on3;
    bit2.state[i] = on2;
    bit1.state[i] = on1;
    bit0.state[i] = on0;
  }
}

std::array<uint64_t, 4> inline CountNeighbourhoodColumn(const LifeState &state, int column) {
  std::array<uint64_t, 4> nearby;
  for (int i = 0; i < 4; i++) {
    int c = (column + i - 1 + N) % N;
    nearby[i] = state[c];
  }

  std::array<uint64_t, 4> col0;
  std::array<uint64_t, 4> col1;

  for (int i = 0; i < 4; i++) {
    uint64_t a = nearby[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    col0[i] = l ^ r ^ a;
    col1[i] = ((l ^ r) & a) | (l & r);
  }

  std::array<uint64_t, 4> result;

  {
    int idxU = 0;
    int i = 1;
    int idxB = 2;

    uint64_t u_on1 = col1[idxU];
    uint64_t u_on0 = col0[idxU];
    uint64_t c_on1 = col1[i];
    uint64_t c_on0 = col0[i];
    uint64_t l_on1 = col1[idxB];
    uint64_t l_on0 = col0[idxB];

    uint64_t uc0, uc1, uc2, uc_carry0;
    HalfAdd(uc0, uc_carry0, u_on0, c_on0);
    FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

    uint64_t on_carry1, on_carry0;
    HalfAdd(result[3], on_carry0, uc0, l_on0);
    FullAdd(result[2], on_carry1, uc1, l_on1, on_carry0);
    HalfAdd(result[1], result[0], uc2, on_carry1);
  }

  return result;
}

void inline CountNeighbourhoodStrip(
    const std::array<uint64_t, 6> &state,
    std::array<uint64_t, 4> &__restrict__ bit3,
    std::array<uint64_t, 4> &__restrict__ bit2,
    std::array<uint64_t, 4> &__restrict__ bit1,
    std::array<uint64_t, 4> &__restrict__ bit0) {

  std::array<uint64_t, 6> col0;
  std::array<uint64_t, 6> col1;

  for (int i = 0; i < 6; i++) {
    uint64_t a = state[i];
    uint64_t l = RotateLeft(a);
    uint64_t r = RotateRight(a);

    col0[i] = l ^ r ^ a;
    col1[i] = ((l ^ r) & a) | (l & r);
  }

  #pragma clang loop vectorize(enable)
  for (int i = 1; i < 5; i++) {
    int idxU = i-1;
    int idxB = i+1;

    uint64_t u_on1 = col1[idxU];
    uint64_t u_on0 = col0[idxU];
    uint64_t c_on1 = col1[i];
    uint64_t c_on0 = col0[i];
    uint64_t l_on1 = col1[idxB];
    uint64_t l_on0 = col0[idxB];

    uint64_t uc0, uc1, uc2, uc_carry0;
    HalfAdd(uc0, uc_carry0, u_on0, c_on0);
    FullAdd(uc1, uc2, u_on1, c_on1, uc_carry0);

    uint64_t on_carry1, on_carry0;
    HalfAdd(bit0[i-1], on_carry0, uc0, l_on0);
    FullAdd(bit1[i-1], on_carry1, uc1, l_on1, on_carry0);
    HalfAdd(bit2[i-1], bit3[i-1], uc2, on_carry1);
  }
}

template <uint32_t max>
class LifeCountdown {
public:
  static constexpr uint32_t lmax = (max == 0) ? 0 : 32 - __builtin_clz(max);
  LifeState started;
  LifeState finished;
  std::array<LifeState, lmax> counter = {0};
  uint32_t n;

  LifeCountdown() : started{}, finished{}, counter{}, n{0} {};
  LifeCountdown(uint32_t n) : started{}, finished{}, counter{}, n{n} {};

  void Start(const LifeState &state) {
    LifeState newStarted = state & ~started;
    for (unsigned i = 0; i < lmax; i++) {
      if ((n >> i) & 1) {
        counter[i] |= newStarted;
      }
    }
    started |= state;
  }

  void Reset(const LifeState &state) {
    for (unsigned i = 0; i < lmax; i++) {
      counter[i] &= ~state;
    }
    started &= ~state;
  }

  void Tick() {
    auto carry = started;
    for (unsigned i = 0; i < lmax; i++) {
        counter[i] ^= carry;
        carry &= counter[i];
    }
    finished |= carry;
  }
};
