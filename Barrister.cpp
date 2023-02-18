#include <cassert>

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"

const unsigned lookaheadGens = 10;
const unsigned maxActive = 5;
const unsigned maxInteractionWindow = 6;
const unsigned stableTime = 2;

class SearchState {
public:

  LifeState starting;
  LifeStableState stable;
  LifeUnknownState current;
  std::vector<LifeUnknownState> lookahead;

  unsigned gen;
  bool hasInteracted;
  unsigned interactionStart;
  unsigned recoveredTime;

  // // Cells in this generation that need to be determined
  // LifeState newUnknown;
  // LifeState newGlancing;
  // Focus focus;

  SearchState();
  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  void TransferStableToCurrent();
  bool TryAdvance();
  bool TryAdvanceOne();
  void PopulateLookahead();

  // std::pair<int, int> UnknownNeighbour(std::pair<int, int> cell);

  std::pair<int, int> ChooseFocus();
  bool CheckAdvance();
  bool CheckConditions();

  void FindFocus();

  void Search();
  void SearchStep();

  void SanityCheck();
};

// std::string SearchState::LifeBellmanRLE() const {
//   LifeState state = stable | params.activePattern;
//   LifeState marked =  unknown | stable;
//   return LifeBellmanRLEFor(state, marked);
// }

SearchState::SearchState() : gen {0}, hasInteracted{false}, interactionStart{0}, recoveredTime{0} {
  lookahead = std::vector<LifeUnknownState>(lookaheadGens);
}

void SearchState::TransferStableToCurrent() {
  // Places that in current are unknownStable might be updated now
  LifeState updated = current.unknownStable & ~stable.unknownStable;
  current.state |= stable.state & updated;
  current.unknown &= ~updated;
  current.unknownStable &= ~updated;
}

bool SearchState::TryAdvance() {
  bool didAdvance;
  do {
    didAdvance = TryAdvanceOne();

    if (hasInteracted && gen - interactionStart > maxInteractionWindow)
      return false;

    if (hasInteracted && recoveredTime > stableTime) {
      std::cout << "Winner" << std::endl;
      std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
      LifeState state = starting | stable.state;
      LifeState marked = stable.unknownStable | stable.state;
      std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
    }

  } while (didAdvance);

  return true;
}

bool SearchState::TryAdvanceOne() {
  LifeUnknownState next = current.UncertainStepMaintaining(stable);
  bool fullyKnown = (next.unknown ^ next.unknownStable).IsEmpty();

  if (!fullyKnown)
    return false;

  if (!hasInteracted) {
    LifeState steppedWithoutStable = (current.state & ~stable.state);
    steppedWithoutStable.Step();

    bool isDifferent = !(next.state ^ steppedWithoutStable).IsEmpty();

    if (isDifferent) {
      hasInteracted = true;
      interactionStart = gen;
    }
  }

  current = next;
  gen++;

  if (hasInteracted) {
    LifeState stableZOI = stable.state.ZOI();
    bool isRecovered = ((stable.state ^ current.state) & stableZOI).IsEmpty();
    if (isRecovered)
      recoveredTime++;
    else
      recoveredTime = 0;
  }

  return true;
}

void SearchState::PopulateLookahead() {
  LifeUnknownState gen = current;
  for (int i = 0; i < lookaheadGens; i++) {
    // std::cout << "Gen " << i  << std::endl;
    // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
    // std::cout << LifeBellmanRLEFor(gen.state, gen.unknown) << std::endl;

    lookahead[i] = gen;
    gen = gen.UncertainStepMaintaining(stable);
  }
}

// std::pair<int, int> SearchState::ChooseFocus() {
//   // Idea is: look at the fringe of unknown cells, and choose a useful
//   // cell in the latest generation possible.
//   LifeState fringe = (~stable.unknownStable).ZOI() & stable.unknownStable;
//   LifeState nearFringe = stable.unknownStable.ZOI();

//   for (int i = gens.size() - 1; i >= 1; i--) {
//     LifeUnknownState &gen = gens[i];
//     LifeUnknownState &prev = gens[i-1];

//     LifeState becomeUnknown =
//         (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

//     LifeState focusable = becomeUnknown & nearFringe;

//     if (!focusable.IsEmpty()) {
//       return (focusable.ZOI() & stable.unknownStable).FirstOn();
//     }
//   }
//   return std::make_pair(-1,-1);
// }

std::pair<int, int> SearchState::ChooseFocus() {
  LifeState stableZOI = stable.state.ZOI();

  // // XXX: TODO: this should try the latest generation instead of the earliest?
  // // Try near existing catalyst
  // for (int i = 1; i < lookahead.size(); i++) {
  //   LifeUnknownState &gen = lookahead[i];
  //   LifeUnknownState &prev = lookahead[i-1];

  //   LifeState becomeUnknown =
  //       (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

  //   LifeState focusable = becomeUnknown & stableZOI;

  //   if (!focusable.IsEmpty()) {
  //     auto result = (focusable.ZOI() & stable.unknownStable).FirstOn();
  //     if (result != std::make_pair(-1, -1))
  //       return result;
  //   }
  // }

  // XXX: TODO: better idea, look for focuasble cells where all the
  // unknown neighbours are unknownStable, that will stop us from
  // wasting time on an expanding unknown region

  for (int i = 3; i >= 1; i--) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown =
        (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    LifeState focusable = becomeUnknown & stableZOI & ~nearActiveUnknown;

    if (!focusable.IsEmpty()) {
      auto result = (focusable.ZOI() & stable.unknownStable).FirstOn();
      if (result != std::make_pair(-1, -1))
        return result;
    }

    // focusable = becomeUnknown & ~nearActiveUnknown;

    // if (!focusable.IsEmpty()) {
    //   return (focusable.ZOI() & stable.unknownStable).FirstOn();
    // }
  }

  // Try anything
  for (int i = 1; i < lookahead.size(); i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown =
        (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

    LifeState focusable = becomeUnknown & stableZOI;

    if (!focusable.IsEmpty()) {
      auto result = (focusable.ZOI() & stable.unknownStable).FirstOn();
      if (result != std::make_pair(-1, -1))
        return result;
    }

    focusable = becomeUnknown;

    if (!focusable.IsEmpty()) {
      auto result = (focusable.ZOI() & stable.unknownStable).FirstOn();
      if (result != std::make_pair(-1, -1))
        return result;
    }
  }

  return std::make_pair(-1,-1);
}

bool SearchState::CheckConditions() {
  for (auto gen : lookahead) {
    LifeState active = gen.ActiveComparedTo(stable);

    if (active.GetPop() > maxActive)
      return false;
  }

  return true;
}

void SearchState::SanityCheck() {
  assert((current.unknownStable & ~current.unknown).IsEmpty());
  assert((stable.state & stable.unknownStable).IsEmpty());
  assert((current.unknownStable & ~stable.unknownStable).IsEmpty());
}

void SearchState::Search() {
  current.state = starting | stable.state;
  current.unknown = stable.unknownStable;
  current.unknownStable = stable.unknownStable;

  SearchStep();
}

void SearchState::SearchStep() {
  bool consistent = stable.PropagateStable();
  if (!consistent) {
    //    std::cout << "not consistent" << std::endl;
    return;
  }

  TransferStableToCurrent();

  if (!TryAdvance()) {
    //std::cout << "advance failed" << std::endl;
    return;
  }

  PopulateLookahead();

  SanityCheck();

  if (!CheckConditions()) {
    //std::cout << "conditions failed" << std::endl;
    return;
  }

  // std::cout << "Stable" << std::endl;
  // LifeState state = starting | stable.state;
  // LifeState marked = stable.unknownStable | stable.state;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
  // std::cout << "Current" << std::endl;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(current.state, current.unknown) << std::endl;

  auto focus = ChooseFocus();
  if (focus == std::make_pair(-1, -1)) {
    std::cout << "no focus" << std::endl;

    return;
  }

  {
    bool which = true;
    SearchState nextState = *this;

    nextState.stable.state.SetCellUnsafe(focus, which);
    nextState.stable.unknownStable.Erase(focus);

    nextState.SearchStep();
  }
  {
    bool which = false;
    SearchState &nextState = *this;

    nextState.stable.state.SetCellUnsafe(focus, which);
    nextState.stable.unknownStable.Erase(focus);

    nextState.SearchStep();
  }
}

int main(int argc, char *argv[]) {

  std::string rle = "x = 29, y = 30, rule = LifeHistory\n\
8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.21B$8.\n\
21B$3.2A3.21B$2.A2.A2.21B$3.3A2.21B$8.21B$8.21B$29B$29B$29B$29B$29B$\n\
29B$29B$29B$29B$29B$29B$29B$29B!\n";

  LifeState on;
  LifeState marked;
  ParseTristateWHeader(rle, on, marked);

  SearchState search;
  search.starting = on;
  search.stable.state = LifeState();
  search.stable.unknownStable = marked;

  search.Search();
}
