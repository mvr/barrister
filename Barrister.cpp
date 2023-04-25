#include <cassert>

#include "toml/toml.hpp"

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "Bits.hpp"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"
#include "Params.hpp"

const unsigned maxLookaheadGens = 6;
const unsigned maxAncientGens = 16-1;

struct FocusSet {
  LifeState focuses;
  LifeState glanceable;

  LifeUnknownState currentState;
  unsigned currentGen;
  bool isForcedInactive;
};


class SearchState {
public:

  LifeStableState stable;
  LifeUnknownState current;

  LifeState everActive;

  FocusSet pendingFocuses;

  LifeCountdown<maxAncientGens> activeTimer;

  std::pair<int, int> focus;

  unsigned currentGen;
  bool hasInteracted;
  unsigned interactionStart;
  unsigned recoveredTime;

  SearchParams *params;
  std::vector<LifeState> *allSolutions;

  SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions);
  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  void TransferStableToCurrent();
  void TransferStableToCurrentColumn(unsigned column);
  bool TryAdvance();
  bool TryAdvanceOne();
  std::tuple<bool, std::array<LifeUnknownState, maxLookaheadGens>, int> PopulateLookahead();

  FocusSet FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, unsigned lookaheadSize) const;

  bool CheckConditionsOn(unsigned gen, const LifeUnknownState &state, const LifeState &active, const LifeState &everActive) const;
  bool CheckConditions(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, unsigned lookaheadSize);

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

SearchState::SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions)
  : currentGen{0}, hasInteracted{false}, interactionStart{0}, recoveredTime{0} {

  params = &inparams;
  allSolutions = &outsolutions;

  stable.state = inparams.startingStable;
  stable.unknownStable = inparams.searchArea;

  current.state = inparams.activePattern | stable.state;
  current.unknown = stable.unknownStable;
  current.unknownStable = stable.unknownStable;

  everActive = LifeState();
  focus = {-1, -1};
  pendingFocuses.focuses = LifeState();
  activeTimer = LifeCountdown<maxAncientGens>(params->maxCellActiveWindowGens);
}

void SearchState::TransferStableToCurrent() {
  // Places that in current are unknownStable might be updated now
  LifeState updated = current.unknownStable & ~stable.unknownStable;
  current.state |= stable.state & updated;
  current.unknown &= ~updated;
  current.unknownStable &= ~updated;

  LifeState focusesUpdated = pendingFocuses.currentState.unknownStable & ~stable.unknownStable;
  pendingFocuses.currentState.state |= stable.state & focusesUpdated;
  pendingFocuses.currentState.unknown &= ~focusesUpdated;
  pendingFocuses.currentState.unknownStable &= ~focusesUpdated;
}

void SearchState::TransferStableToCurrentColumn(unsigned column) {
  for (unsigned i = 0; i < 6; i++) {
    int c = (column + (int)i - 2 + N) % N;
    uint64_t updated = current.unknownStable[c] & ~stable.unknownStable[c];
    current.state[c] |= stable.state[c] & updated;
    current.unknown[c] &= ~updated;
    current.unknownStable[c] &= ~updated;

    uint64_t focusesUpdated = pendingFocuses.currentState.unknownStable[c] & ~stable.unknownStable[c];
    pendingFocuses.currentState.state[c] |= stable.state[c] & focusesUpdated;
    pendingFocuses.currentState.unknown[c] &= ~focusesUpdated;
    pendingFocuses.currentState.unknownStable[c] &= ~focusesUpdated;
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

    if (params->maxCellActiveWindowGens != -1) {
      activeTimer.Start(active);
      activeTimer.Tick();
    }

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

FocusSet SearchState::FindFocuses(std::array<LifeUnknownState, maxLookaheadGens> &lookahead, unsigned lookaheadSize) const {
  LifeState activeRect(false);
  if (params->activeBounds.first == -1) {
    activeRect= LifeState();
  } else {
    activeRect = ~LifeState::SolidRect(
      -params->activeBounds.first + 1, -params->activeBounds.second + 1,
      2 * params->activeBounds.first - 1, 2 * params->activeBounds.second - 1);
  }

  LifeState everActiveForcedInactive(false);
  if (params->everActiveBounds.first == -1) {
    everActiveForcedInactive = LifeState();
  } else {
    LifeState everActiveRect = ~LifeState::SolidRect(
      -params->everActiveBounds.first + 1, -params->everActiveBounds.second + 1,
      2 * params->everActiveBounds.first - 1, 2 * params->everActiveBounds.second - 1);
    everActiveForcedInactive = everActive.Convolve(everActiveRect);
  }


  std::array<LifeState, maxLookaheadGens> allFocusable;
  std::array<LifeState, maxLookaheadGens> allForcedInactive;
  for (unsigned i = 1; i < lookaheadSize; i++) {
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~(prev.unknown & ~prev.unknownStable);
    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    // TODO: This was already computed when populating the lookahead,
    // shouldn't have to recompute
    LifeState active = gen.ActiveComparedTo(stable);

    allFocusable[i] = becomeUnknown & ~nearActiveUnknown;

    // IDEA: calculate 'forcedInactive' cells, where any cell that is
    // active will break one of the constraints

    LifeState activeForcedInactive(false);
    if (params->maxActiveCells != -1 && (active.GetPop() == (unsigned)params->maxActiveCells || currentGen + i < params->minFirstActiveGen)) {
      activeForcedInactive = ~active;
    } else if (params->activeBounds.first != -1) {
      activeForcedInactive = active.Convolve(activeRect);
    } else {
      activeForcedInactive = LifeState();
    }

    allForcedInactive[i] = activeForcedInactive | everActiveForcedInactive;
  }

  // IDEA: look for focusable cells where all the unknown neighbours
  // are unknownStable, that will stop us from wasting time on an
  // expanding unknown region

  // LifeState oneStableUnknownNeighbour  =  stable.unknown0 & ~stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  // LifeState twoStableUnknownNeighbours = ~stable.unknown0 &  stable.unknown1 & ~stable.unknown2 & ~stable.unknown3;
  LifeState oneOrTwoUnknownNeighbours  = (stable.unknown0 ^ stable.unknown1) & ~stable.unknown2 & ~stable.unknown3;
  // LifeState fewStableUnknownNeighbours = ~stable.unknown2 & ~stable.unknown3;

#define TRY_CHOOSE(exp, isprio)                                         \
  for (unsigned i = 1; i < lookaheadSize; i++) {                        \
    LifeState focusable = allFocusable[i];                              \
    LifeState forcedInactive = allForcedInactive[i];                    \
    focusable &= exp;                                                   \
    if (!focusable.IsEmpty())                                           \
      return {focusable, lookahead[i].glanceableUnknown,                \
              lookahead[i - 1], currentGen + i - 1, isprio};            \
  }

  TRY_CHOOSE(stable.stateZOI & forcedInactive & oneOrTwoUnknownNeighbours, true);
  TRY_CHOOSE(forcedInactive & oneOrTwoUnknownNeighbours, true);
  TRY_CHOOSE(stable.stateZOI & forcedInactive, true);
  TRY_CHOOSE(forcedInactive, true);

  TRY_CHOOSE(stable.stateZOI & oneOrTwoUnknownNeighbours, false);
  TRY_CHOOSE(oneOrTwoUnknownNeighbours, false);
  TRY_CHOOSE(stable.stateZOI, false);

  // Try anything at all
  TRY_CHOOSE(~LifeState(), false);

#undef TRY_CHOOSE

  // This shouldn't be reached
  return {LifeState(), LifeState(), LifeUnknownState(), 0, false};
}

bool SearchState::CheckConditionsOn(unsigned gen, const LifeUnknownState &state, const LifeState &active, const LifeState &everActive) const {
  auto activePop = active.GetPop();

  if (gen < params->minFirstActiveGen && activePop > 0)
    return false;

  if (params->maxActiveCells != -1 && activePop > (unsigned)params->maxActiveCells)
    return false;

  if(hasInteracted && gen > interactionStart + params->maxActiveWindowGens && activePop > 0)
    return false;

  if (params->maxCellActiveWindowGens != -1 && currentGen > (unsigned)params->maxCellActiveWindowGens && !(active & activeTimer.finished).IsEmpty())
    return false;

  if(params->activeBounds.first != -1) {
    auto wh = active.WidthHeight();
    if (wh.first > params->activeBounds.first || wh.second > params->activeBounds.second)
      return false;
  }

  if (params->maxEverActiveCells != -1 && everActive.GetPop() > (unsigned)params->maxEverActiveCells)
    return false;

  if(params->everActiveBounds.first != -1) {
    auto wh = everActive.WidthHeight();
    if (wh.first > params->everActiveBounds.first || wh.second > params->everActiveBounds.second)
      return false;
  }

  if (params->hasStator && !(~state.state & params->stator & ~state.unknown).IsEmpty())
      return false;

  return true;
}

std::tuple<bool, std::array<LifeUnknownState, maxLookaheadGens>, int> SearchState::PopulateLookahead() {
  auto lookahead = std::array<LifeUnknownState, maxLookaheadGens>();
  lookahead[0] = current;
  unsigned i;
  for (i = 0; i < maxLookaheadGens-1; i++) {
    lookahead[i+1] = lookahead[i].UncertainStepMaintaining(stable);

    LifeState active = lookahead[i+1].ActiveComparedTo(stable);

    everActive |= active;
    bool genResult = CheckConditionsOn(currentGen + i, lookahead[i+1], active, everActive);

    if(!genResult)
      return {false, lookahead, i+2};

    if(active.IsEmpty())
      return {true, lookahead, i+2};
  }

  if (hasInteracted) {
    LifeUnknownState gen = lookahead[maxLookaheadGens - 1];
    for(unsigned i = maxLookaheadGens; currentGen + i <= interactionStart + params->maxActiveWindowGens + 1; i++) {
      gen = gen.UncertainStepFast(stable);
      LifeState active = gen.ActiveComparedTo(stable);
      everActive |= active;

      if(active.IsEmpty())
        break;

      bool genResult = CheckConditionsOn(currentGen + i, gen, active, everActive);
      if (!genResult)
        return {false, lookahead, maxLookaheadGens};
    }
  }

  return {true, lookahead, maxLookaheadGens};
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
  SearchStep();
}

void SearchState::SearchStep() {
  if (focus == std::pair(-1, -1) && pendingFocuses.focuses.IsEmpty()) {
    bool consistent = stable.PropagateStable();
    if (!consistent)
      return;

    TransferStableToCurrent();

    if (!TryAdvance())
      return;

    auto [passed, lookahead, lookaheadSize] = PopulateLookahead();

    if (!passed)
      return;

    pendingFocuses = FindFocuses(lookahead, lookaheadSize);

    // SanityCheck();
  }

  if(focus == std::pair(-1, -1)) {
    focus = (pendingFocuses.focuses & ~pendingFocuses.glanceable).FirstOn();
    if (focus == std::pair(-1, -1)) {
      focus = pendingFocuses.focuses.FirstOn();
      // Shouldn't be possible
      if (focus == std::pair(-1, -1)) {
        std::cout << "no focus" << std::endl;
        exit(1);
      }
    }

    bool focusIsGlancing =
        params->skipGlancing && pendingFocuses.glanceable.Get(focus) &&
        pendingFocuses.currentState.StillGlancingFor(focus, stable);

    if(focusIsGlancing) {
      pendingFocuses.glanceable.Erase(focus);

      if (!pendingFocuses.isForcedInactive || stable.unknown2.Get(focus) ||
          stable.unknown3.Get(focus)) { // TODO: handle overpopulation better

        SearchState nextState = *this;
        nextState.stable.glancedON.Set(focus);
        nextState.SearchStep();
      }

      stable.glanced.Set(focus);
      pendingFocuses.focuses.Erase(focus);
      focus = {-1, -1};

      [[clang::musttail]]
      return SearchStep();
    }
  }

  bool focusIsDetermined = pendingFocuses.currentState.KnownNext(focus);

  auto cell = stable.UnknownNeighbour(focus);
  if (focusIsDetermined || cell == std::pair(-1, -1)) {
    pendingFocuses.focuses.Erase(focus);
    focus = {-1, -1};

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

    bool consistent = nextState.stable.PropagateColumn(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.pendingFocuses.currentState.UncertainStepFast(nextState.stable);
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

    bool consistent = nextState.stable.PropagateColumn(cell.first);
    if(consistent) {
      nextState.TransferStableToCurrentColumn(cell.first);
      LifeUnknownState quicklook = nextState.pendingFocuses.currentState.UncertainStepFast(nextState.stable);
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
  LifeState starting = params->activePattern;
  LifeState state = starting | stable.state;
  LifeState marked = stable.unknownStable | stable.state;
  std::cout << LifeBellmanRLEFor(state, marked) << std::endl;

  if(params->stabiliseResults) {
    LifeState completed = stable.CompleteStable();

    if(!completed.IsEmpty()){
      std::cout << "Completed:" << std::endl;
      std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
      LifeState remainingHistory = stable.unknownStable & ~completed.ZOI().MooreZOI(); // ZOI().MooreZOI() gives a BigZOI without the diagonals
      LifeState stator = params->stator | (stable.state & ~everActive) | (completed & ~stable.state);
      LifeHistoryState history(starting | completed, remainingHistory , LifeState(), stator);
      std::cout << history.RLE() << std::endl;

      std::cout << "Completed Plain:" << std::endl;
      std::cout << (completed | starting).RLE() << std::endl;
      allSolutions->push_back(completed | starting);
    } else {
      std::cout << "Completion failed!" << std::endl;
      std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
      LifeHistoryState history;
      std::cout << history.RLE() << std::endl;
      std::cout << "Completed Plain:" << std::endl;
      std::cout << LifeState().RLE() << std::endl;
    }
  }
}

void PrintSummary(std::vector<LifeState> &pats) {
  std::cout << "Summary:" << std::endl;
  std::cout << "x = 0, y = 0, rule = B3/S23" << std::endl;
  for (unsigned i = 0; i < pats.size(); i += 8) {
    std::vector<LifeState> row =
      std::vector<LifeState>(pats.begin() + i, pats.begin() + std::min((unsigned)pats.size(), i + 8));
    std::cout << RowRLE(row) << std::endl;
  }
}

int main(int, char *argv[]) {
  auto toml = toml::parse(argv[1]);
  SearchParams params = SearchParams::FromToml(toml);

  std::vector<LifeState> allSolutions;

  SearchState search(params, allSolutions);
  search.Search();

  if (params.printSummary)
    PrintSummary(allSolutions);
}
