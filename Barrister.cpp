#include <cassert>

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"

const unsigned maxLookaheadGens = 10;
const unsigned maxLocalGens = 3;
const unsigned maxActive = 5;
const unsigned maxEverActive = 10;
const unsigned maxInteractionWindow = 6;
const unsigned stableTime = 2;

class SearchState {
public:

  LifeState starting;
  LifeStableState stable;
  LifeUnknownState current;
  // std::vector<LifeUnknownState> lookahead;

  LifeState pendingFocuses;
  LifeState everActive;
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
  std::vector<LifeUnknownState> PopulateLookahead();

  // std::pair<int, int> UnknownNeighbour(std::pair<int, int> cell);
  bool CheckAdvance();

  LifeState FindFocuses(std::vector<LifeUnknownState> &lookahead);
  std::pair<int, int> ChooseFocus();

  bool CheckConditionsOn(LifeState &active, LifeState &everActive);
  bool CheckConditions(std::vector<LifeUnknownState> &lookahead);

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
  everActive = LifeState();
  pendingFocuses = LifeState();
}

void SearchState::TransferStableToCurrent() {
  // Places that in current are unknownStable might be updated now
  LifeState updated = current.unknownStable & ~stable.unknownStable;
  current.state |= stable.state & updated;
  current.unknown &= ~updated;
  current.unknownStable &= ~updated;
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

bool SearchState::TryAdvance() {
  bool didAdvance;
  do {
    didAdvance = TryAdvanceOne();

    LifeState active = current.ActiveComparedTo(stable);
    everActive |= active;

    if (!CheckConditionsOn(active, everActive)) {
      return false;
    }

    if (hasInteracted && gen - interactionStart > maxInteractionWindow)
      return false;

    if (hasInteracted && recoveredTime > stableTime) {
      LifeState completed = stable.CompleteStable();

      std::cout << "Winner:" << std::endl;
      std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
      LifeState state = starting | stable.state;
      LifeState marked = stable.unknownStable | stable.state;
      std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
      std::cout << "Completed:" << std::endl;
      std::cout << (completed | starting).RLE() << std::endl;

      return false;
    }

  } while (didAdvance);

  return true;
}

std::vector<LifeUnknownState> SearchState::PopulateLookahead() {
  auto lookahead = std::vector<LifeUnknownState>();
  lookahead.reserve(maxLookaheadGens);
  LifeUnknownState gen = current;
  lookahead.push_back(gen);
  for (int i = 0; i < maxLookaheadGens; i++) {
    // std::cout << "Gen " << i  << std::endl;
    // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
    // std::cout << LifeBellmanRLEFor(gen.state, gen.unknown) << std::endl;

    gen = gen.UncertainStepMaintaining(stable);
    lookahead.push_back(gen);

    LifeState active = gen.ActiveComparedTo(stable);
    if(active.IsEmpty())
      break;
  }
  return lookahead;
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

LifeState SearchState::FindFocuses(std::vector<LifeUnknownState> &lookahead) {
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

  for (int i = std::min((unsigned)maxLocalGens, (unsigned)lookahead.size())-1; i >= 1; i--) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown =
        (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    LifeState focusable = becomeUnknown & stableZOI & ~nearActiveUnknown;

    if (!focusable.IsEmpty()) {
      return focusable;
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
      return focusable;
    }

    focusable = becomeUnknown;

    if (!focusable.IsEmpty()) {
      return focusable;
    }
  }

  return LifeState();
}

bool SearchState::CheckConditionsOn(LifeState &active, LifeState &everActive) {
  if (active.GetPop() > maxActive)
    return false;

  auto wh = active.WidthHeight();
  // std::cout << wh.first << ", " << wh.second << std::endl;
  int maxDim = std::max(wh.first, wh.second);
  if (maxDim > maxActiveSize)
    return false;

  if (everActive.GetPop() > maxEverActive)
    return false;

  wh = everActive.WidthHeight();
  maxDim = std::max(wh.first, wh.second);
  if (maxDim > maxEverActiveSize)
    return false;

  return true;
}

bool SearchState::CheckConditions(std::vector<LifeUnknownState> &lookahead) {
  LifeState newEverActive = everActive;
  for (auto gen : lookahead) {
    LifeState active = gen.ActiveComparedTo(stable);

    newEverActive |= active;
    bool genResult = CheckConditionsOn(active, newEverActive);
    if (!genResult)
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
  if(pendingFocuses.IsEmpty()) {
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

    if(!hasInteracted && gen > maxStartTime)
      return;

    // std::cout << "Stable" << std::endl;
    // LifeState state = starting | stable.state;
    // LifeState marked = stable.unknownStable | stable.state;
    // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
    // std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
    // std::cout << "Current" << std::endl;
    // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
    // std::cout << LifeBellmanRLEFor(current.state, current.unknown) << std::endl;

    std::vector<LifeUnknownState> lookahead = PopulateLookahead();

    if (!CheckConditions(lookahead)) {
      //std::cout << "conditions failed" << std::endl;
      return;
    }

    pendingFocuses = FindFocuses(lookahead);

    SanityCheck();
  }

  auto focus = pendingFocuses.FirstOn();
  if (focus == std::make_pair(-1, -1)) {
    // Shouldn't be possible
    std::cout << "no focus" << std::endl;
    exit(1);
  }

  // TODO: we want to know whether our choices have fixed the focus, without setting the entire neighbourhood
  // bool focusIsDetermined = ?.KnownNext(focus);

  auto cell = stable.UnknownNeighbour(focus);
  if(cell == std::make_pair(-1, -1)) {
    pendingFocuses.Erase(focus);
    SearchStep();
    return;
  }

  {
    bool which = true;
    SearchState nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent)
      nextState.SearchStep();
  }
  {
    bool which = false;
    SearchState &nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent)
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
