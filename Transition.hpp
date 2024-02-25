#pragma once

#include <type_traits>
// Maybe this should just be a bitset + constants

enum class Transition : unsigned char {
  OFF_TO_OFF       = 1 << 0,
  OFF_TO_ON        = 1 << 1,
  ON_TO_OFF        = 1 << 2,
  ON_TO_ON         = 1 << 3,
  STABLE_TO_STABLE = 1 << 4,

  IMPOSSIBLE       = 0,
  ANY = OFF_TO_OFF | OFF_TO_ON | ON_TO_OFF | ON_TO_ON | STABLE_TO_STABLE,
  UNCHANGING = OFF_TO_OFF | ON_TO_ON | STABLE_TO_STABLE,
};

constexpr inline Transition operator~ (Transition a) { return static_cast<Transition>( ~static_cast<std::underlying_type<Transition>::type>(a) ); }
constexpr inline Transition operator| (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) | static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition operator& (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) & static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition operator^ (Transition a, Transition b) { return static_cast<Transition>( static_cast<std::underlying_type<Transition>::type>(a) ^ static_cast<std::underlying_type<Transition>::type>(b) ); }
constexpr inline Transition& operator|= (Transition& a, Transition b) { a = a | b; return a; }
constexpr inline Transition& operator&= (Transition& a, Transition b) { a = a & b; return a; }
constexpr inline Transition& operator^= (Transition& a, Transition b) { a = a ^ b; return a; }

Transition TransitionLowest(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return static_cast<Transition>(bits & (~bits + 1));
}

Transition TransitionHighest(Transition t) {
  if (t == Transition::IMPOSSIBLE) return Transition::IMPOSSIBLE;
  unsigned int bits = static_cast<unsigned int>(t);
  return static_cast<Transition>(static_cast<unsigned char>(1 << (8*sizeof(bits)-1-__builtin_clz(bits))));
}

bool TransitionIsSingleton(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return bits && !(bits & (bits-1));
}

unsigned TransitionCount(Transition t) {
  unsigned char bits = static_cast<unsigned char>(t);
  return __builtin_popcount(bits);
}

Transition TransitionSimplify(Transition transition) {
  bool has_on  = (transition & Transition::ON_TO_ON)   == Transition::ON_TO_ON;
  bool has_off = (transition & Transition::OFF_TO_OFF) == Transition::OFF_TO_OFF;
  bool has_stable = (transition & Transition::STABLE_TO_STABLE) == Transition::STABLE_TO_STABLE;

  // No need to branch them separately
  if (has_on && has_off) {
    transition |= Transition::STABLE_TO_STABLE;
    transition &= ~(Transition::ON_TO_ON | Transition::OFF_TO_OFF);
  }
  if (has_on && !has_off) transition &= ~Transition::STABLE_TO_STABLE;
  if (!has_on && has_off) transition &= ~Transition::STABLE_TO_STABLE;
  if (has_stable) transition &= ~(Transition::ON_TO_ON | Transition::OFF_TO_OFF);

  return transition;
}
