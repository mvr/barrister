#include <cassert>

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"

const unsigned maxLookaheadGens = 10;
const unsigned maxLocalGens = 7;

const unsigned maxEverActive = 100;
const unsigned maxStartTime = 10;

const unsigned maxActive = 100;
const unsigned maxActiveSize = 5;
const unsigned maxEverActiveSize = 5;

// const unsigned maxActive = 15;
// const unsigned maxActiveSize = 7;
// const unsigned maxEverActiveSize = 7;

const unsigned maxInteractionWindow = 15;
const unsigned stableTime = 2;

class SearchState {
public:

  LifeState starting;
  LifeStableState stable;
  LifeUnknownState current;

  LifeState pendingFocuses;
  LifeState pendingGlanceable;

  LifeUnknownState focusCurrent;

  LifeState everActive;

  unsigned focusCurrentGen;

  unsigned currentGen;
  bool hasInteracted;
  unsigned interactionStart;
  unsigned recoveredTime;

  SearchState();
  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  void TransferStableToCurrent();
  void TransferStableToCurrentColumn(int column);
  bool TryAdvance();
  bool TryAdvanceOne();
  std::pair<std::array<LifeUnknownState, maxLookaheadGens>, int> PopulateLookahead() const;

  //std::pair<LifeState, LifeUnknownState> FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) const;
  std::tuple<LifeState, LifeState, LifeUnknownState, unsigned> FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) const;

  bool CheckConditionsOn(int gen, LifeUnknownState &state, LifeState &active, LifeState &everActive) const;
  bool CheckConditions(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize);

  void Search();
  void SearchStep();

  bool ContainsEater2(LifeState &stable, LifeState &everActive) const;
  void ReportSolution();

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

void SearchState::TransferStableToCurrentColumn(int column) {
  for (int i = 0; i < 5; i++) {
    int c = (column + i - 2 + N) % N;
    uint64_t updated = current.unknownStable.state[c] & ~stable.unknownStable.state[c];
    current.state.state[c] |= stable.state.state[c] & updated;
    current.unknown.state[c] &= ~updated;
    current.unknownStable.state[c] &= ~updated;
  }
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
  while (didAdvance = TryAdvanceOne(), didAdvance) {
    LifeState active = current.ActiveComparedTo(stable);
    everActive |= active;

    if (!CheckConditionsOn(currentGen, current, active, everActive)) {
      return false;
    }

    if (hasInteracted && currentGen - interactionStart > maxInteractionWindow)
      return false;

    if (hasInteracted && recoveredTime > stableTime) {
      ReportSolution();
      return false;
    }
  }

  return true;
}

std::pair<std::array<LifeUnknownState, maxLookaheadGens>, int> SearchState::PopulateLookahead() const {
  auto lookahead = std::array<LifeUnknownState, maxLookaheadGens>();
  lookahead[0] = current;
  int i;
  for (i = 0; i < maxLookaheadGens-1; i++) {
    lookahead[i+1] = lookahead[i].UncertainStepMaintaining(stable);

    LifeState active = lookahead[i+1].ActiveComparedTo(stable);
    if(active.IsEmpty())
      return {lookahead, i+2};
  }
  return {lookahead, maxLookaheadGens};
}

std::tuple<LifeState, LifeState, LifeUnknownState, unsigned> SearchState::FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) const {
  std::array<LifeState, maxLookaheadGens> allFocusable;
  for (int i = 1; i < lookaheadSize; i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~(prev.unknown & ~prev.unknownStable);
    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    // LifeState prevActive = prev.unknown & ~prev.unknownStable;
    // LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~prevActive;
    // LifeState nearActiveUnknown = prevActive.ZOI();

    allFocusable[i] = becomeUnknown & ~nearActiveUnknown;
  }

  // IDEA: calculate a 'priority' area, for example cells outside
  // the permitted 'everActive' area

  const LifeState rect = LifeState::SolidRect(- maxEverActiveSize+1, - maxEverActiveSize+1, 2 * maxEverActiveSize - 1, 2 * maxEverActiveSize - 1);
  const LifeState priority = everActive.Convolve(~rect);

  // IDEA: look for focusable cells where all the unknown neighbours
  // are unknownStable, that will stop us from wasting time on an
  // expanding unknown region

  LifeState oneStableUnknownNeighbour  =  stable.unknown0 & ~stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  LifeState twoStableUnknownNeighbours = ~stable.unknown0 &  stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  // LifeState fewStableUnknownNeighbours = ~stable.unknown2 & ~stable.unknown3;

#define TRY_CHOOSE(exp) \
  for (int i = 1; i < lookaheadSize; i++) { \
    LifeState focusable = allFocusable[i]; \
    focusable &= exp; \
    if (!focusable.IsEmpty()) \
      return {focusable, lookahead[i].glanceableUnknown, lookahead[i-1], i-1}; \
  }

  TRY_CHOOSE(stable.stateZOI & priority & (oneStableUnknownNeighbour | twoStableUnknownNeighbours));
  TRY_CHOOSE(priority & (oneStableUnknownNeighbour | twoStableUnknownNeighbours));
  TRY_CHOOSE(stable.stateZOI & priority)
  TRY_CHOOSE(priority)

  TRY_CHOOSE(stable.stateZOI & (oneStableUnknownNeighbour | twoStableUnknownNeighbours));
  TRY_CHOOSE(oneStableUnknownNeighbour | twoStableUnknownNeighbours);
  TRY_CHOOSE(stable.stateZOI);

#undef TRY_CHOOSE

  // Try anything at all
  for (int i = 1; i < lookaheadSize; i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];
    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~(prev.unknown & ~prev.unknownStable);
    LifeState focusable = becomeUnknown;
    // LifeState focusable = allFocusable[i];

    if (!focusable.IsEmpty())
      return {focusable, lookahead[i].glanceableUnknown, lookahead[i-1], i-1};
  }

  // This shouldn't be reached
  return {LifeState(), LifeState(), LifeUnknownState(), 0};
}

bool SearchState::CheckConditionsOn(int gen, LifeUnknownState &state, LifeState &active, LifeState &everActive) const {
  if (active.GetPop() > maxActive)
    return false;

  auto wh = active.WidthHeight();
  int maxDim = std::max(wh.first, wh.second);
  if (maxDim > maxActiveSize)
    return false;

  if (everActive.GetPop() > maxEverActive)
    return false;

  wh = everActive.WidthHeight();
  maxDim = std::max(wh.first, wh.second);
  if (maxDim > maxEverActiveSize)
    return false;

  if(hasInteracted && gen > interactionStart + maxInteractionWindow && !active.IsEmpty())
    return false;

  return true;
}

bool SearchState::CheckConditions(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) {
  for (int i = 0; i < lookaheadSize; i++) {
    LifeUnknownState &gen = lookahead[i];

    LifeState active = gen.ActiveComparedTo(stable);

    everActive |= active;
    bool genResult = CheckConditionsOn(currentGen + i, gen, active, everActive);

    if (!genResult)
      return false;
  }
  // This could miss a catalyst recovering then failing
  if (hasInteracted) {
    LifeUnknownState gen = lookahead[lookaheadSize - 1];
    for(int i = lookaheadSize; currentGen + i < interactionStart + maxInteractionWindow; i++) {
      gen = gen.UncertainStepMaintaining(stable);
      LifeState active = gen.ActiveComparedTo(stable);
      everActive |= active;
      bool genResult = CheckConditionsOn(currentGen + i, gen, active, everActive);
      if (!genResult)
        return false;
    }
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

    auto [lookahead, lookaheadSize] = PopulateLookahead();

    if (!CheckConditions(lookahead, lookaheadSize)) {
      //std::cout << "conditions failed" << std::endl;
      return;
    }

    std::tie(pendingFocuses, pendingGlanceable, focusCurrent, focusCurrentGen) = FindFocuses(lookahead, lookaheadSize); // C++ wtf

    SanityCheck();
  }

  auto focus = (pendingFocuses & ~pendingGlanceable).FirstOn();
  if (focus == std::pair(-1, -1)) {
    focus = pendingFocuses.FirstOn();
    // Shouldn't be possible
    if (focus == std::pair(-1, -1)) {
      std::cout << "no focus" << std::endl;
      exit(1);
    }
  }

  bool focusIsGlancing = pendingGlanceable.Get(focus) && focusCurrent.StillGlancingFor(focus, stable);
  if(focusIsGlancing) {
    pendingGlanceable.Erase(focus);

    SearchState nextState = *this;
    nextState.stable.glancedON.Set(focus);
    nextState.SearchStep();

    pendingFocuses.Erase(focus);
    stable.glanced.Set(focus);
    SearchStep();
    return;
  }

  bool focusIsDetermined = focusCurrent.KnownNext(focus);

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

    nextState.focusCurrent.state.SetCellUnsafe(cell, which);
    nextState.focusCurrent.unknown.Erase(cell);
    nextState.focusCurrent.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.focusCurrent.UncertainStepMaintaining(nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed = CheckConditionsOn(focusCurrentGen+1, quicklook, quickactive, quickeveractive);
      if(conditionsPassed)
        nextState.SearchStep();
    }
  }
  {
    bool which = false;
    SearchState &nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.focusCurrent.state.SetCellUnsafe(cell, which);
    nextState.focusCurrent.unknown.Erase(cell);
    nextState.focusCurrent.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.focusCurrent.UncertainStepMaintaining(nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed = CheckConditionsOn(focusCurrentGen+1, quicklook, quickactive, quickeveractive);
      if(conditionsPassed)
        nextState.SearchStep();
    }
  }
}

bool SearchState::ContainsEater2(LifeState &stable, LifeState &everActive) const {
  LifeState blockMatch;
  for(unsigned i = 0; i < N-1; ++i)
    blockMatch.state[i] = stable.state[i] & RotateRight(stable.state[i]) &
      stable.state[i+1] & RotateRight(stable.state[i+1]);
  blockMatch.state[N-1] = stable.state[N-1] & RotateRight(stable.state[N-1]) &
    stable.state[0] & RotateRight(stable.state[0]);

  std::vector<LifeState> shouldBeActive = {
      LifeState::Parse("bo$o!", 1, 1),
      LifeState::Parse("o$bo!", -1, 1),
      LifeState::Parse("bo$o!", -1, -1),
      LifeState::Parse("o$bo!", 1, -1),
  };
  std::vector<LifeState> shouldNotBeActive = {
    LifeState::Parse("2bo2$obo!", 0, 0),
    LifeState::Parse("o2$obo!", -1, 0),
    LifeState::Parse("obo2$o!", -1, -1),
    LifeState::Parse("obo2$2bo!", 0, -1),
  };

  while (!blockMatch.IsEmpty()) {
    auto corner = blockMatch.FirstOn();
    blockMatch.Erase(corner);
    for(unsigned i = 0; i < 4; ++i){
      LifeState shouldBeActiveCopy = shouldBeActive[i];
      LifeState shouldNotBeActiveCopy = shouldNotBeActive[i];
      shouldBeActiveCopy.Move(corner);
      shouldNotBeActiveCopy.Move(corner);

      if(everActive.Contains(shouldBeActiveCopy)
         && everActive.AreDisjoint(shouldNotBeActiveCopy)){
        return true;
      }
    }
  }
  return false;
}

void SearchState::ReportSolution() {
  if (ContainsEater2(stable.state, everActive))
    return;

  LifeState completed = stable.CompleteStable();

  std::cout << "Winner:" << std::endl;
  std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  LifeState state = starting | stable.state;
  LifeState marked = stable.unknownStable | stable.state;
  std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(state, stable.glanced) << std::endl;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(state, stable.glancedON) << std::endl;m
  std::cout << "Completed:" << std::endl;
  std::cout << (completed | starting).RLE() << std::endl;
}

int main(int argc, char *argv[]) {
  std::string rle = "x = 27, y = 30, rule = LifeHistory\n\
6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.\n\
21B$.2A3.21B$A2.A2.21B$.3A2.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$\n\
6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B$6.21B!\n";

  LifeState on;
  LifeState marked;
  ParseTristateWHeader(rle, on, marked);

  SearchState search;
  search.starting = on;
  search.stable.state = LifeState();
  search.stable.unknownStable = marked;

  search.Search();
}
