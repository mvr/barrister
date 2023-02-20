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
  LifeUnknownState focusGeneration;
  LifeState everActive;

  unsigned currentGen;
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
  std::vector<LifeUnknownState> PopulateLookahead() const;

  std::pair<LifeState, LifeUnknownState> FindFocuses(std::vector<LifeUnknownState> &lookahead) const;

  bool CheckConditionsOn(LifeState &active, LifeState &everActive) const;
  bool CheckConditions(std::vector<LifeUnknownState> &lookahead) const;

  void Search();
  void SearchStep();

  void SanityCheck();
};

// std::string SearchState::LifeBellmanRLE() const {
//   LifeState state = stable | params.activePattern;
//   LifeState marked =  unknown | stable;
//   return LifeBellmanRLEFor(state, marked);
// }

SearchState::SearchState() : currentGen {0}, hasInteracted{false}, interactionStart{0}, recoveredTime{0} {
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
      interactionStart = currentGen;
    }
  }

  current = next;
  currentGen++;

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

    if (hasInteracted && currentGen - interactionStart > maxInteractionWindow)
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

std::vector<LifeUnknownState> SearchState::PopulateLookahead() const {
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

std::pair<LifeState, LifeUnknownState> SearchState::FindFocuses(std::vector<LifeUnknownState> &lookahead) const {
  // XXX: TODO: calculate a 'priority' area, for example cells outside
  // the permitted 'everActive' area

  // IDEA: look for focuasble cells where all the unknown neighbours
  // are unknownStable, that will stop us from wasting time on an
  // expanding unknown region

  // for (int i = std::min((unsigned)maxLocalGens, (unsigned)lookahead.size())-1; i >= 1; i--) {
  //   LifeUnknownState &gen = lookahead[i];
  //   LifeUnknownState &prev = lookahead[i-1];

  //   LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

  //   LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

  //   LifeState oneStableUnknownNeighbour  =  stable.unknown0 & ~stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  //   LifeState twoStableUnknownNeighbours = ~stable.unknown0 & stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;

  //   // LifeState focusable = becomeUnknown & stable.stateZOI & ~nearActiveUnknown & oneStableUnknownNeighbour;
  //   // LifeState focusable = becomeUnknown & stable.stateZOI & ~nearActiveUnknown & (oneStableUnknownNeighbour | twoStableUnknownNeighbours);
  //   LifeState focusable = becomeUnknown & stable.stateZOI & ~nearActiveUnknown;

  //   // LifeState focusable = becomeUnknown & ~nearActiveUnknown;

  //   if (!focusable.IsEmpty()) {
  //     return focusable;
  //   }
  // }

  // LifeState hasUnknownNeighbour = stable.unknown0 | stable.unknown1 | stable.unknown2 | stable.unknown3;

  // Try anything
  for (int i = 1; i < lookahead.size(); i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

    //    LifeState focusable = becomeUnknown & hasUnknownNeighbour & stable.stateZOI;
    // LifeState focusable = becomeUnknown & stable.stateZOI;

    // if (!focusable.IsEmpty()) {
    //   return focusable;
    // }

    //focusable = becomeUnknown & hasUnknownNeighbour;
    LifeState focusable = becomeUnknown;

    if (!focusable.IsEmpty()) {
      return {focusable, prev};
    }
  }

  // for (int i = 1; i < lookahead.size(); i++) {
  //   LifeUnknownState &gen = lookahead[i];
  //   LifeUnknownState &prev = lookahead[i-1];

  //   LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~prev.unknown;

  //   LifeState focusable = becomeUnknown & hasUnknownNeighbour;

  //   if (!focusable.IsEmpty()) {
  //     return focusable;
  //   }
  // }

  return {LifeState(), LifeUnknownState()};
}

bool SearchState::CheckConditionsOn(LifeState &active, LifeState &everActive) const {
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

bool SearchState::CheckConditions(std::vector<LifeUnknownState> &lookahead) const {
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

    if(!hasInteracted && currentGen > maxStartTime)
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

    std::tie(pendingFocuses, focusGeneration) = FindFocuses(lookahead); // C++ wtf

    SanityCheck();
  }

  auto focus = pendingFocuses.FirstOn();
  if (focus == std::pair(-1, -1)) {
    // Shouldn't be possible
    std::cout << "no focus" << std::endl;
    exit(1);
  }

  bool focusIsDetermined = focusGeneration.KnownNext(focus);

  auto cell = stable.UnknownNeighbour(focus);
  if(focusIsDetermined || cell == std::pair(-1, -1)) {
    pendingFocuses.Erase(focus);
    SearchStep();
    return;
  }

  {
    bool which = true;
    SearchState nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.focusGeneration.state.SetCellUnsafe(cell, which);
    nextState.focusGeneration.unknown.Erase(cell);
    nextState.focusGeneration.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent)
      nextState.SearchStep();
  }
  {
    bool which = false;
    SearchState &nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.focusGeneration.state.SetCellUnsafe(cell, which);
    nextState.focusGeneration.unknown.Erase(cell);
    nextState.focusGeneration.unknownStable.Erase(cell);

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
