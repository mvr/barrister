#include <cassert>

#include "toml/toml.hpp"

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"
#include "Params.hpp"

const unsigned maxLookaheadGens = 6;

struct FocusSet {
  LifeState focuses;
  LifeState glanceable;
  LifeState priority;

  LifeUnknownState currentState;
  unsigned currentGen;
};


class SearchState {
public:

  LifeState starting;
  LifeStableState stable;
  LifeUnknownState current;

  LifeState everActive;

  FocusSet pendingFocuses;

  unsigned currentGen;
  bool hasInteracted;
  unsigned interactionStart;
  unsigned recoveredTime;

  SearchParams *params;

  SearchState(SearchParams &inparams);
  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  void TransferStableToCurrent();
  void TransferStableToCurrentColumn(int column);
  bool TryAdvance();
  bool TryAdvanceOne();
  std::pair<std::array<LifeUnknownState, maxLookaheadGens>, int> PopulateLookahead() const;

  FocusSet FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) const;

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

SearchState::SearchState(SearchParams &inparams)
  : currentGen{0}, hasInteracted{false}, interactionStart{0}, recoveredTime{0} {

  params = &inparams;

  starting = inparams.activePattern | inparams.startingStable;
  stable.state = inparams.startingStable;
  stable.unknownStable = inparams.searchArea;

  everActive = LifeState();
  pendingFocuses.focuses = LifeState();
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
    uint64_t updated = current.unknownStable[c] & ~stable.unknownStable[c];
    current.state[c] |= stable.state[c] & updated;
    current.unknown[c] &= ~updated;
    current.unknownStable[c] &= ~updated;
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
    bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();
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

    if (!CheckConditionsOn(currentGen, current, active, everActive))
      return false;

    if(!hasInteracted && currentGen > params->maxFirstActiveGen)
      return false;

    if (hasInteracted && currentGen > interactionStart + params->maxActiveWindowGens && recoveredTime == 0)
      return false;

    if (hasInteracted && currentGen < params->minFirstActiveGen)
      return false;

    if (hasInteracted && recoveredTime > params->minStableInterval) {
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

FocusSet SearchState::FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, int lookaheadSize) const {
  const LifeState activeRect = ~LifeState::SolidRect(
      -params->activeBounds.first + 1, -params->activeBounds.second + 1,
      2 * params->activeBounds.first - 1, 2 * params->activeBounds.second - 1);

  const LifeState everActiveRect = ~LifeState::SolidRect(
      -params->everActiveBounds.first + 1, -params->everActiveBounds.second + 1,
      2 * params->everActiveBounds.first - 1, 2 * params->everActiveBounds.second - 1);
  const LifeState everActivePriority = everActive.Convolve(everActiveRect);

  std::array<LifeState, maxLookaheadGens> allFocusable;
  std::array<LifeState, maxLookaheadGens> allPriority;
  for (int i = 1; i < lookaheadSize; i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~(prev.unknown & ~prev.unknownStable);
    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    // LifeState prevActive = prev.unknown & ~prev.unknownStable;
    // LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~prevActive;
    // LifeState nearActiveUnknown = prevActive.ZOI();

    // This is being recomputed
    LifeState active = gen.ActiveComparedTo(stable);

    allFocusable[i] = becomeUnknown & ~nearActiveUnknown;

    // IDEA: calculate a 'priority' area, for example cells outside
    // the permitted 'everActive' area

    allPriority[i] = active.Convolve(activeRect) | everActivePriority;
    if (active.GetPop() == params->maxActiveCells - 1) {
      // Any active cell at all will
      allPriority[i] = ~LifeState();
    }
  }

  // IDEA: look for focusable cells where all the unknown neighbours
  // are unknownStable, that will stop us from wasting time on an
  // expanding unknown region

  // LifeState oneStableUnknownNeighbour  =  stable.unknown0 & ~stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  // LifeState twoStableUnknownNeighbours = ~stable.unknown0 &  stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  LifeState oneOrTwoUnknownNeighbours  = (stable.unknown0 ^ stable.unknown1) & ~stable.unknown2 & ~stable.unknown3;
  // LifeState fewStableUnknownNeighbours = ~stable.unknown2 & ~stable.unknown3;

#define TRY_CHOOSE(exp)                                                        \
  for (int i = 1; i < lookaheadSize; i++) {                                    \
    LifeState focusable = allFocusable[i];                                     \
    LifeState priority = allPriority[i];                                       \
    focusable &= exp;                                                          \
    if (!focusable.IsEmpty())                                                  \
      return {focusable, lookahead[i].glanceableUnknown, priority,             \
              lookahead[i - 1], currentGen + i - 1};                           \
  }

  TRY_CHOOSE(stable.stateZOI & priority & oneOrTwoUnknownNeighbours);
  TRY_CHOOSE(priority & oneOrTwoUnknownNeighbours);
  TRY_CHOOSE(stable.stateZOI & priority)
  TRY_CHOOSE(priority)

  TRY_CHOOSE(stable.stateZOI & oneOrTwoUnknownNeighbours);
  TRY_CHOOSE(oneOrTwoUnknownNeighbours);
  TRY_CHOOSE(stable.stateZOI);

  // Try anything at all
  TRY_CHOOSE(~LifeState());

#undef TRY_CHOOSE

  // This shouldn't be reached
  return {LifeState(), LifeState(), LifeState(), LifeUnknownState(), 0};
}

bool SearchState::CheckConditionsOn(int gen, LifeUnknownState &state, LifeState &active, LifeState &everActive) const {
  auto activePop = active.GetPop();

  if (gen < params->minFirstActiveGen && activePop > 0)
    return false;

  if (activePop > params->maxActiveCells)
    return false;

  if(hasInteracted && gen > interactionStart + params->maxActiveWindowGens && activePop > 0)
    return false;

  auto wh = active.WidthHeight();
  if (wh.first > params->activeBounds.first || wh.second > params->activeBounds.second)
    return false;

  if (everActive.GetPop() > params->maxEverActiveCells)
    return false;

  wh = everActive.WidthHeight();
  if (wh.first > params->everActiveBounds.first || wh.second > params->everActiveBounds.second)
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
  // TODO: This could miss a catalyst recovering then failing
  if (hasInteracted) {
    LifeUnknownState gen = lookahead[lookaheadSize - 1];
    for(int i = lookaheadSize; currentGen + i < interactionStart + params->maxActiveWindowGens; i++) {
      gen = gen.UncertainStepMaintaining(stable);
      LifeState active = gen.ActiveComparedTo(stable);
      everActive |= active;

      if(active.IsEmpty())
        break;

      bool genResult = CheckConditionsOn(currentGen + i, gen, active, everActive);
      if (!genResult)
        return false;
    }
  }

  return true;
}

void SearchState::SanityCheck() {
  assert((stable.unknownStable & stable.glanced).IsEmpty());
  assert((stable.unknownStable & stable.glancedON).IsEmpty());
  assert((stable.state & stable.glanced).IsEmpty());
  assert((stable.state & stable.glancedON).IsEmpty());
  assert((stable.unknownStable & stable.glanced).IsEmpty());
  assert((stable.unknownStable & stable.glancedON).IsEmpty());
  assert((stable.glanced & stable.glancedON).IsEmpty());

  assert((current.unknownStable & ~current.unknown).IsEmpty());
  assert((stable.state & stable.unknownStable).IsEmpty());
  assert((current.unknownStable & ~stable.unknownStable).IsEmpty());

  //assert((~pendingFocuses & pendingGlanceable).IsEmpty());

}

void SearchState::Search() {
  current.state = starting | stable.state;
  current.unknown = stable.unknownStable;
  current.unknownStable = stable.unknownStable;

  SearchStep();
}

void SearchState::SearchStep() {
  if(pendingFocuses.focuses.IsEmpty()) {
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

    pendingFocuses = FindFocuses(lookahead, lookaheadSize);

    // SanityCheck();
  }

  auto focus = (pendingFocuses.focuses & ~pendingFocuses.glanceable).FirstOn();
  if (focus == std::pair(-1, -1)) {
    focus = pendingFocuses.focuses.FirstOn();
    // Shouldn't be possible
    if (focus == std::pair(-1, -1)) {
      std::cout << "no focus" << std::endl;
      exit(1);
    }
  }

  bool focusIsGlancing = params->skipGlancing && pendingFocuses.glanceable.Get(focus) && pendingFocuses.currentState.StillGlancingFor(focus, stable);
  if(focusIsGlancing) {
    pendingFocuses.glanceable.Erase(focus);

    if (!pendingFocuses.priority.Get(focus) || stable.unknown2.Get(focus) ||
        stable.unknown3.Get(focus)) { // TODO: handle overpopulation better
      SearchState nextState = *this;
      nextState.stable.glancedON.Set(focus);
      nextState.SearchStep();
    }

    pendingFocuses.focuses.Erase(focus);
    stable.glanced.Set(focus);

    [[clang::musttail]]
    return SearchStep();
  }

  bool focusIsDetermined = pendingFocuses.currentState.KnownNext(focus);

  auto cell = stable.UnknownNeighbour(focus);
  if(focusIsDetermined || cell == std::pair(-1, -1)) {
    pendingFocuses.focuses.Erase(focus);

    [[clang::musttail]]
    return SearchStep();
  }

  {
    bool which = true;
    SearchState nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.pendingFocuses.currentState.state.SetCellUnsafe(cell, which);
    nextState.pendingFocuses.currentState.unknown.Erase(cell);
    nextState.pendingFocuses.currentState.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.pendingFocuses.currentState.UncertainStepMaintaining(nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed = CheckConditionsOn(pendingFocuses.currentGen+1, quicklook, quickactive, quickeveractive);
      if(conditionsPassed)
        nextState.SearchStep();
    }
  }
  {
    bool which = false;
    SearchState &nextState = *this;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.pendingFocuses.currentState.state.SetCellUnsafe(cell, which);
    nextState.pendingFocuses.currentState.unknown.Erase(cell);
    nextState.pendingFocuses.currentState.unknownStable.Erase(cell);

    bool consistent = nextState.stable.SimplePropagateColumnStep(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.pendingFocuses.currentState.UncertainStepMaintaining(nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed = CheckConditionsOn(pendingFocuses.currentGen+1, quicklook, quickactive, quickeveractive);
      if(conditionsPassed)
        [[clang::musttail]]
        return nextState.SearchStep();
    }
  }
}

bool SearchState::ContainsEater2(LifeState &stable, LifeState &everActive) const {
  LifeState blockMatch;
  for(unsigned i = 0; i < N-1; ++i)
    blockMatch[i] = stable[i] & RotateRight(stable[i]) &
      stable[i+1] & RotateRight(stable[i+1]);
  blockMatch[N-1] = stable[N-1] & RotateRight(stable[N-1]) &
    stable[0] & RotateRight(stable[0]);

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
  if (params->forbidEater2 && ContainsEater2(stable.state, everActive))
    return;

  std::cout << "Winner:" << std::endl;
  std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  LifeState state = starting | stable.state;
  LifeState marked = stable.unknownStable | stable.state;
  std::cout << LifeBellmanRLEFor(state, marked) << std::endl;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(state, stable.glanced) << std::endl;
  // std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  // std::cout << LifeBellmanRLEFor(state, stable.glancedON) << std::endl;m
  if(params->stabiliseResults) {
    LifeState completed = stable.CompleteStable();
    std::cout << "Completed:" << std::endl;
    std::cout << (completed | starting).RLE() << std::endl;
  }
}

int main(int argc, char *argv[]) {


  auto toml = toml::parse(argv[1]);
  SearchParams params = SearchParams::FromToml(toml);

  SearchState search(params);
  search.Search();
}
