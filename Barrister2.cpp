#include <cassert>

#include "toml/toml.hpp"

#include "Bits.hpp"
#include "LifeAPI.h"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"
#include "Params.hpp"
#include "Parsing.hpp"

// Idea:
//
// * Calculate the frontier

// * Go through it a single cell at a time

// * For each cell, determine which of the cases (ON to OFF, OFF to
// ON, STABLE to STABLE) are allowed. If there is only one then set it
// and remove the cell from the frontier

// * If we set any cell of the frontier, then propagate stable and
// start over

// * Once there are only true choices left, branch on a cell in the
// frontier (earliest first?)

// After branching, we will likely get some new frontier cells, and
// some previous frontier cells might become settable. Maybe we should
// check the existing frontier cells for any settable ones quickly,
// before doing a full recalculation. (Maybe only if we have made the
// new cell active instead of stable)

// It might also be useful to calculate the "semi-frontier" while
// calculating the frontier, and using that information somehow when
// choosing which cell to branch on. Perhaps something like: branch on
// the frontier cell whose ZOI (over all generations) touches the most
// semi-frontier cells

// The UnknownStep could be made more intelligent by handling unknown
// active cells in the neighbourhood. E.g., if we are a DEAD6 cell
// then we will stay dead.

// It may not be worth the cost: if there is an unknown active cell
// then the current cell is likely to be swamped in the next
// generation or two

const unsigned maxFrontierGens = 4;
const unsigned maxBranchingGens = maxFrontierGens;
const unsigned maxBranchFastCount = 1;
const unsigned maxCalculateRounds = 1;
const unsigned maxFastLookaheadGens = 3;

const unsigned maxCellActiveWindowGens = 0;
const unsigned maxCellActiveStreakGens = 0;

struct FrontierGeneration {
  LifeUnknownState prev;
  LifeUnknownState state;
  LifeState frontierCells;
  LifeState active;
  LifeState changes;
  LifeState forcedInactive;
  LifeState forcedUnchanging;

  unsigned gen;

  void SetTransition(std::pair<int, int> cell, Transition transition) {
    prev.SetTransitionPrev(cell, transition);
    state.SetTransitionResult(cell, transition);
  }

  std::string RLE() const {
    LifeHistoryState history(state.state, state.unknown & ~state.unknownStable,
                             state.unknownStable, LifeState());
    return history.RLEWHeader();
  }
};

Transition AllowedTransitions(bool state, bool unknownstable, bool stablestate,
                              bool forcedInactive, bool forcedUnchanging, bool inzoi, Transition unperturbed) {
  auto result = Transition::ANY & ~Transition::STABLE_TO_STABLE;

  // If current state is known, remove options with the wrong previous state
  if (!unknownstable && state)
    result &= Transition::ON_TO_OFF | Transition::ON_TO_ON;
  if (!unknownstable && !state)
    result &= Transition::OFF_TO_OFF | Transition::OFF_TO_ON;

  if (forcedInactive && inzoi) {
    if (unknownstable)
      result &= ~(Transition::OFF_TO_ON | Transition::ON_TO_OFF);
    if (!unknownstable && stablestate)
      result &= ~(Transition::OFF_TO_OFF | Transition::ON_TO_OFF);
    if (!unknownstable && !stablestate)
      result &= ~(Transition::OFF_TO_ON | Transition::ON_TO_ON);
  }

  if (forcedInactive && !inzoi) {
    if (unknownstable) {
      result &= Transition::UNCHANGING;
    }
    if (!unknownstable) {
      result &= unperturbed;
    }
  }

  if (forcedUnchanging && inzoi)
    result &= Transition::OFF_TO_OFF | Transition::ON_TO_ON;

  return result;
}

Transition AllowedTransitions(const FrontierGeneration &generation,
                              const LifeStableState &stable,
                              std::pair<int, int> cell) {
  auto allowedTransitions = AllowedTransitions(
      generation.prev.state.Get(cell), stable.unknown.Get(cell),
      stable.state.Get(cell), generation.forcedInactive.Get(cell),
      generation.forcedUnchanging.Get(cell), stable.stateZOI.Get(cell),
      generation.prev.UnperturbedTransitionFor(cell));

  return allowedTransitions;


  // auto possibleTransitions = generation.prev.TransitionsFor(stable, cell);
  // return possibleTransitions & allowedTransitions;
}


struct Frontier {
  std::array<FrontierGeneration, maxFrontierGens> generations;
  unsigned start;
  unsigned size;
};

class SearchState {
public:
  LifeStableState stable;
  LifeUnknownState current;

  Frontier frontier;

  LifeState everActive;

  LifeCountdown<maxCellActiveWindowGens> activeTimer;
  LifeCountdown<maxCellActiveStreakGens> streakTimer;

  LifeStableState lastTest;

  unsigned timeSincePropagate;

  unsigned currentGen;

  bool hasInteracted;
  unsigned interactionStart;
  unsigned recoveredTime;

  SearchParams *params;
  std::vector<LifeState> *allSolutions;

  SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions);
  SearchState(const SearchState &) = default;
  SearchState &operator=(const SearchState &) = default;

  LifeState ForcedInactiveCells(
      unsigned gen, const LifeUnknownState &state,
      const LifeStableState &stable,
      const LifeState &active, const LifeState &everActive,
      const LifeState &changes
      // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
                                ) const;
  LifeState ForcedUnchangingCells(
      unsigned gen, const LifeUnknownState &state,
      const LifeStableState &stable,
      const LifeState &active, const LifeState &everActive,
      const LifeState &changes
      // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
          ) const;

  bool UpdateActive(FrontierGeneration &generation);
  std::pair<bool, bool> SetForced(FrontierGeneration &generation);

  std::pair<bool, bool> TestActive(FrontierGeneration &generation);

  bool FastLookahead();
  std::tuple<bool, bool> PopulateFrontier();
  bool CalculateFrontier();
  std::pair<bool, bool> TryAdvance();

  StableOptions OptionsFor(const LifeUnknownState &state,
                           std::pair<int, int> cell,
                           Transition transition) const;
  void ResolveFrontier();

  std::pair<unsigned, std::pair<int, int>> ChooseBranchCell() const;

  void SearchStep();

  void ReportSolution();

  void SanityCheck();
};

LifeState SearchState::ForcedInactiveCells(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeState &active,
    const LifeState &everActive, const LifeState &changes
    // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
    // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
        ) const {
  if (gen < params->minFirstActiveGen) {
    return ~LifeState();
  }

  auto activePop = active.GetPop();

  if (hasInteracted && !params->reportOscillators && gen > interactionStart + params->maxActiveWindowGens) {
    return ~LifeState();
  }

  if (params->maxActiveCells != -1 &&
      activePop > (unsigned)params->maxActiveCells)
    return ~LifeState();

  LifeState result;

  if (params->maxActiveCells != -1 && activePop == (unsigned)params->maxActiveCells)
    result |= ~active; // Or maybe just return

  if (params->activeBounds.first != -1 && activePop > 0) {
    result |= ~active.BufferAround(params->activeBounds);
  }

  if (params->maxEverActiveCells != -1 &&
      everActive.GetPop() == (unsigned)params->maxEverActiveCells) {
    result |= ~everActive; // Or maybe just return
  }

  if (params->everActiveBounds.first != -1 && activePop > 0) {
    result |= ~everActive.BufferAround(params->everActiveBounds);
  }

  return result;
}
LifeState SearchState::ForcedUnchangingCells(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeState &active,
    const LifeState &everActive, const LifeState &changes
    // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
    // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
        ) const {
  return LifeState();
}

StableOptions OptionsFor(bool currentstate, bool nextstate, unsigned currenton,
                         unsigned unknown, unsigned stableon) {
  // The possible current count for the neighbourhood, as a bitfield
  unsigned currentmask = (1 << 9) - 1;

  currentmask &= ((1 << (unknown + 1)) - 1) << currenton;

  if (!currentstate && !nextstate) currentmask &= 0b111110111;
  if (!currentstate &&  nextstate) currentmask &= 0b000001000;
  if ( currentstate && !nextstate) currentmask &= 0b111110011;
  if ( currentstate &&  nextstate) currentmask &= 0b000001100;

  // The possible stable count for the neighbourhood, as a bitfield
  unsigned stablemask = (currentmask >> currenton) << stableon;

  StableOptions result = StableOptions::IMPOSSIBLE;
  if ((1 << 2) & stablemask) result |= StableOptions::LIVE2;
  if ((1 << 3) & stablemask) result |= StableOptions::LIVE3;
  if ((1 << 0) & stablemask) result |= StableOptions::DEAD0;
  if ((1 << 1) & stablemask) result |= StableOptions::DEAD1;
  if ((1 << 2) & stablemask) result |= StableOptions::DEAD2;
  if ((1 << 4) & stablemask) result |= StableOptions::DEAD4;
  if ((1 << 5) & stablemask) result |= StableOptions::DEAD5;
  if ((1 << 6) & stablemask) result |= StableOptions::DEAD6;
  return result;
}

StableOptions SearchState::OptionsFor(const LifeUnknownState &state,
                                      std::pair<int, int> cell,
                                      Transition transition) const {
  unsigned currenton = state.state.CountNeighbours(cell);
  unsigned unknown = state.unknown.CountNeighbours(cell);
  unsigned stableon = stable.state.CountNeighbours(cell);

  StableOptions options;
  switch (transition) {
  case Transition::OFF_TO_OFF:
    options = ::OptionsFor(false, false, currenton, unknown, stableon);
    if (state.unknownStable.Get(cell))
      options &= StableOptions::DEAD;
    break;
  case Transition::OFF_TO_ON:
    options = ::OptionsFor(false, true, currenton, unknown, stableon);
    if (state.unknownStable.Get(cell))
      options &= StableOptions::DEAD;
    break;
  case Transition::ON_TO_OFF:
    options = ::OptionsFor(true, false, currenton, unknown, stableon);
    if (state.unknownStable.Get(cell))
      options &= StableOptions::LIVE;
    break;
  case Transition::ON_TO_ON:
    options = ::OptionsFor(true, true, currenton, unknown, stableon);
    if (state.unknownStable.Get(cell))
      options &= StableOptions::LIVE;
    break;
  case Transition::STABLE_TO_STABLE:
    options = (::OptionsFor(false, false, currenton, unknown, stableon) & StableOptions::DEAD) |
              (::OptionsFor(true,  true,  currenton, unknown, stableon) & StableOptions::LIVE);
    break;
  default:
    options = StableOptions::IMPOSSIBLE;
  }

  return options;
}

bool SearchState::UpdateActive(FrontierGeneration &generation) {
  generation.active = generation.state.ActiveComparedTo(stable) & stable.stateZOI;
  generation.changes = generation.state.ChangesComparedTo(generation.prev) & stable.stateZOI;

  everActive |= generation.active;

  generation.forcedInactive = ForcedInactiveCells(
      generation.gen, generation.state, stable,
      generation.active, everActive, generation.changes);

  if (!(generation.active & generation.forcedInactive).IsEmpty())
    return false;

  generation.forcedUnchanging = ForcedUnchangingCells(
      generation.gen, generation.state, stable,
      generation.active, everActive, generation.changes);

  if (!(generation.changes & generation.forcedUnchanging).IsEmpty())
    return false;

  return true;
}

std::pair<bool, bool> SearchState::SetForced(FrontierGeneration &generation) {
  bool anyChanges = false;
  LifeState remainingCells = generation.frontierCells;
  for (auto cell = remainingCells.FirstOn(); cell != std::make_pair(-1, -1);
       remainingCells.Erase(cell), cell = remainingCells.FirstOn()) {

    stable.SynchroniseStateKnown(cell);
    generation.prev.TransferStable(stable, cell);
    generation.state.TransferStable(stable, cell);

    auto allowedTransitions = AllowedTransitions(generation, stable, cell);

    if (allowedTransitions == Transition::IMPOSSIBLE) {
      return {false, false};
    }

    auto startingOptions = stable.GetOptions(cell);
    auto possibleOptions = StableOptions::IMPOSSIBLE;

    // See which options and transitions can be actually realised
    auto remainingTransitions = allowedTransitions;
    for (auto transition = TransitionHighest(remainingTransitions);
         remainingTransitions != Transition::IMPOSSIBLE;
         remainingTransitions &= ~transition, transition = TransitionHighest(remainingTransitions)) {

      auto transitionOptions = startingOptions & OptionsFor(generation.prev, cell, transition);
      possibleOptions |= transitionOptions;
      if (transitionOptions == StableOptions::IMPOSSIBLE)
        allowedTransitions &= ~transition;
    }

    auto newOptions = possibleOptions & startingOptions;
    if (newOptions == StableOptions::IMPOSSIBLE)
      return {false, false};

    stable.RestrictOptions(cell, newOptions);
    stable.SynchroniseStateKnown(cell);

    bool alreadyFixed = startingOptions == newOptions;
    if (!alreadyFixed) {
      anyChanges = true;
    }

    allowedTransitions = TransitionSimplify(allowedTransitions);

    // Force the transition to occur if it is the only one allowed
    if (TransitionIsSingleton(allowedTransitions)) {
      generation.SetTransition(cell, allowedTransitions);
      generation.frontierCells.Erase(cell);

      // stable.SanityCheck();
      // generation.prev.SanityCheck(stable);
      // generation.state.SanityCheck(stable);
    }
  }

  return {true, anyChanges};
}

bool SearchState::FastLookahead() {
  LifeUnknownState prev = current;
  LifeUnknownState generation;
  unsigned gen = currentGen;

  for (unsigned i = 0; i < maxFastLookaheadGens; i++) {
    gen++;

    generation = prev.StepMaintaining(stable);

    LifeState active = generation.ActiveComparedTo(stable) & stable.stateZOI;
    LifeState changes = generation.ChangesComparedTo(prev) & stable.stateZOI;

    everActive |= active;

    LifeState forcedInactive = ForcedInactiveCells(
        gen, generation, stable, active, everActive, changes);

    if (!(active & forcedInactive).IsEmpty())
      return false;

    LifeState forcedUnchanging = ForcedUnchangingCells(
      gen, generation, stable,
      active, everActive, changes);

    if (!(changes & forcedUnchanging).IsEmpty())
      return false;

    bool alive =
        !((prev.state ^ generation.state) & ~prev.unknown & ~generation.unknown)
             .IsEmpty();

    if (!alive)
      break;

    prev = generation;
  }
  return true;
}

std::tuple<bool, bool> SearchState::PopulateFrontier() {
  bool anyChanges = false;

  current.TransferStable(stable);

  frontier.start = 0;
  frontier.size = 0;

  LifeUnknownState stepped = current;
  unsigned gen = currentGen;

  for (unsigned i = 0; i < maxFrontierGens; i++) {
    gen++;

    auto &generation = frontier.generations[frontier.size];
    frontier.size++;

    generation.prev = stepped;
    generation.state = stepped.StepMaintaining(stable);
    generation.gen = gen;

    bool updateresult = UpdateActive(generation);
    if (!updateresult)
      return {false, false};

    LifeState prevUnknownActive = generation.prev.unknown & ~generation.prev.unknownStable;
    LifeState becomeUnknown = (generation.state.unknown & ~generation.state.unknownStable) & ~prevUnknownActive;

    generation.frontierCells = becomeUnknown & ~prevUnknownActive.ZOI();

    auto [result, someForced] = SetForced(generation);
    if (!result)
      return {false, false};
    anyChanges = anyChanges || someForced;

    bool isInert = ((generation.prev.state ^ generation.state.state) &
                   ~generation.prev.unknown & ~generation.state.unknown)
                      .IsEmpty() ||
                  (stable.stateZOI & ~generation.state.unknown).IsEmpty();

    if (isInert)
      break;

    stepped = generation.state;
  }
  return {true, anyChanges};
}

std::pair<bool, bool> SearchState::TryAdvance() {
  bool didAdvance = false;
  while (frontier.size > 0) {
    auto &generation = frontier.generations[frontier.start];

    if (!(generation.state.unknown & ~generation.state.unknownStable).IsEmpty())
      break;

    didAdvance = true;

    current = generation.state;
    currentGen += 1;

    frontier.start++;
    frontier.size--;

    if (hasInteracted) {
      bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();

      if (isRecovered)
        recoveredTime++;
      else
        recoveredTime = 0;

      if (!isRecovered && currentGen > interactionStart + params->maxActiveWindowGens) {
        // TODO: This is the place to check for oscillators
        return {false, false};
      }

      if (isRecovered && recoveredTime == params->minStableInterval) {
        ReportSolution();
        return {false, false};
      }
    } else {
      if (currentGen > params->maxFirstActiveGen)
        return {false, false};
    }
  }

  return {true, didAdvance};
}

bool SearchState::CalculateFrontier() {
  current.TransferStable(stable);

  // bool consistent = FastLookahead();
  // if (!consistent) {
  //   return false;
  // }
  bool consistent;

  unsigned rounds = 0;

  bool anyChanges = true;
  while (anyChanges) {
    anyChanges = false;

    rounds++;
    if (rounds > maxCalculateRounds)
      break;

    bool someChanges;
    std::tie(consistent, someChanges) = PopulateFrontier();
    if (!consistent) {
      return false;
    }
    anyChanges = anyChanges || someChanges;

    auto propagateResult = stable.Propagate();
    if (!propagateResult.consistent)
      return false;
    anyChanges = anyChanges || propagateResult.changed;

    // This is more important now than in old Barrister: we otherwise
    // spend a fair bit of time searching uncompletable parts of the
    // search space

    LifeState toTest = stable.Vulnerable().ZOI() & stable.Differences(lastTest).ZOI();
    lastTest = stable;
    propagateResult = stable.TestUnknowns(toTest);
    if (!propagateResult.consistent) {
      return false;
    }
    anyChanges = anyChanges || propagateResult.changed;

    stable.SanityCheck();
  }

  bool didAdvance = false;
  std::tie(consistent, didAdvance) = TryAdvance();
  if (!consistent)
    return false;
  if (didAdvance) {
    if (frontier.size == 0) {
      // We have to start over
      return CalculateFrontier();
    }
  }

  return true;
}

std::pair<unsigned, std::pair<int, int>> SearchState::ChooseBranchCell() const {
  unsigned stopGen = std::min(frontier.size, maxBranchingGens);

  // Prefer unknown stable cells?
  // Prefer lower numbers of possible transitions?
  // Prefer cells where all options are active?
  // for (unsigned i = 0; i < generations.size(); i++) {
  //   std::pair<int, int> branchCell = generations[i].frontierCells.FirstOn();
  //   if (branchCell.first != -1) {
  //     auto allowedTransitions = ::AllowedTransitions(generations[i], stable, branchCell);
  //     if ((allowedTransitions & Transition::STABLE_TO_STABLE) != Transition::STABLE_TO_STABLE)
  //     // if(TransitionCount(allowedTransitions) <= 3)
  //       return {i, branchCell};
  //   }
  // }
  for (unsigned i = frontier.start; i < frontier.start + stopGen; i++) {
    auto &g = frontier.generations[i];
    auto branchCell = g.frontierCells.FirstOn();
    if (branchCell.first != -1)
      return {i, branchCell};
  }

  return {0, {-1, -1}};
}

void SearchState::SearchStep() {
  if(frontier.size == 0 || frontier.generations[frontier.start].frontierCells.IsEmpty() || timeSincePropagate > maxBranchFastCount){
    bool consistent = CalculateFrontier();
    if (!consistent)
      return;
    timeSincePropagate = 0;

    assert(frontier.size > 0);
    stable.SanityCheck();
    // SanityCheck();
  } else {
    auto &generation = frontier.generations[frontier.start];

    generation.prev.TransferStable(stable);
    generation.state.TransferStable(stable);

    bool updateresult = UpdateActive(generation);
    if (!updateresult)
      return;

    timeSincePropagate++;

    auto [consistent, changed] = SetForced(generation);
    if (!consistent)
      return;

    if (generation.frontierCells.IsEmpty()) {
      bool consistent = CalculateFrontier();
      if (!consistent)
        return;
      timeSincePropagate = 0;
    }
  }

  auto [i, branchCell] = ChooseBranchCell();
  assert(branchCell.first != -1);
  auto &generation = frontier.generations[i];

  stable.SynchroniseStateKnown(branchCell);
  generation.prev.TransferStable(stable, branchCell);
  generation.state.TransferStable(stable, branchCell);

  auto allowedTransitions = AllowedTransitions(generation, stable, branchCell);
  allowedTransitions = TransitionSimplify(allowedTransitions);
  // The cell should not still be in the frontier in this case
  assert(allowedTransitions != Transition::IMPOSSIBLE);
  assert(!TransitionIsSingleton(allowedTransitions));

  // Loop over the possible transitions
  for (auto transition = TransitionHighest(allowedTransitions);
       allowedTransitions != Transition::IMPOSSIBLE;
       allowedTransitions &= ~transition, transition = TransitionHighest(allowedTransitions)) {

    auto newoptions = stable.GetOptions(branchCell) & OptionsFor(generation.prev, branchCell, transition);

    if (newoptions == StableOptions::IMPOSSIBLE)
      continue;

    SearchState newSearch = *this;
    newSearch.stable.RestrictOptions(branchCell, newoptions);
    newSearch.stable.SynchroniseStateKnown(branchCell);

    auto propagateResult = newSearch.stable.PropagateStrip(branchCell.first);
    if (!propagateResult.consistent)
      continue;

    newSearch.frontier.generations[i].frontierCells.Erase(branchCell);
    newSearch.frontier.generations[i].SetTransition(branchCell, transition);

    if (generation.prev.TransitionIsPerturbation(branchCell, transition)) {
      newSearch.stable.stateZOI.Set(branchCell);

      if (!hasInteracted) {
        newSearch.hasInteracted = true;
        newSearch.interactionStart = generation.gen;
      }
    }

    newSearch.SearchStep();
  }
  // TODO: Do the last iteration without copying the state, a small performance improvement
  // (Should be identical to the body of the loop, except newSearch is a reference)
  // TODO: move body to a function, make sure it becomes a tail call correctly
  // {
  //   SearchState &newSearch = *this;
  //   ...
  //   [[clang::musttail]]
  //   return newSearch.SearchStep();
  // }
}

SearchState::SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions) : currentGen{0}, hasInteracted{false}, interactionStart{0} {
  params = &inparams;
  allSolutions = &outsolutions;

  stable.state = inparams.startingStable;
  stable.unknown = inparams.searchArea;

  current.state = inparams.startingPattern;
  current.unknown = stable.unknown;
  current.unknownStable = stable.unknown;

  // This needs to be done in this order first, because the counts/options start at all 0
  stable.SynchroniseStateKnown();
  stable.Propagate();
  current.TransferStable(stable);

  frontier = {};
  frontier.start = 0;
  frontier.size = 0;

  timeSincePropagate = 0;

  TryAdvance();

  everActive = LifeState();
  // activeTimer = LifeCountdown<maxCellActiveWindowGens>(params->maxCellActiveWindowGens);
  // streakTimer = LifeCountdown<maxCellActiveStreakGens>(params->maxCellActiveStreakGens);
}

void SearchState::ReportSolution() {
  std::cout << "Winner:" << std::endl;
  std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  LifeState starting = params->startingPattern;
  LifeState startingStableOff = params->startingStable & ~params->startingPattern;
  LifeState state = params->startingPattern | (stable.state & ~startingStableOff);
  LifeState marked = stable.unknown | (stable.state & ~startingStableOff);
  std::cout << LifeBellmanRLEFor(state, marked) << std::endl;

  if(params->stabiliseResults) {
    LifeState completed = stable.CompleteStable(params->stabiliseResultsTimeout, params->minimiseResults);

    if(!completed.IsEmpty()){
      // std::cout << "Completed:" << std::endl;
      // std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
      // LifeState remainingHistory = stable.unknownStable & ~completed.ZOI().MooreZOI(); // ZOI().MooreZOI() gives a BigZOI without the diagonals
      // LifeState stator = params->stator | (stable.state & ~everActive) | (completed & ~stable.state);
      // LifeHistoryState history(starting | (completed & ~startingStableOff), remainingHistory , LifeState(), stator);
      // std::cout << history.RLE() << std::endl;

      std::cout << "Completed Plain:" << std::endl;
      std::cout << ((completed & ~startingStableOff) | starting).RLE() << std::endl;
      allSolutions->push_back((completed & ~startingStableOff) | starting);
    } else {
      // std::cout << "Completion failed!" << std::endl;
      // std::cout << "x = 0, y = 0, rule = LifeHistory" << std::endl;
      // LifeHistoryState history;
      // std::cout << history.RLE() << std::endl;

      std::cout << "Completion Failed!" << std::endl;
      std::cout << LifeState().RLE() << std::endl;
    }
  }
}

void SearchState::SanityCheck() {
#ifdef DEBUG
  current.SanityCheck(stable);
  assert((stable.state & stable.unknown).IsEmpty());

  LifeStableState copy = stable;
  auto result = copy.Propagate();
  assert(result.consistent);
  assert(copy == stable); // The stable state should be fully propagated

  LifeStableState copycopy = copy;
  copycopy.Propagate();
  assert(copycopy == copy);

  LifeUnknownState startingStable = {stable.state, stable.unknown, stable.unknown};
  LifeUnknownState steppedStable = startingStable.StepMaintaining(stable);

  assert(startingStable == steppedStable);
#endif
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

  // if (params.maxCellActiveWindowGens != -1 &&
  // (unsigned)params.maxCellActiveWindowGens > maxCellActiveWindowGens) {
  //   std::cout << "max-cell-active-window is higher than allowed by the
  //   hardcoded value!" << std::endl; exit(1);
  // }
  // if (params.maxCellActiveStreakGens != -1 &&
  // (unsigned)params.maxCellActiveStreakGens > maxCellActiveStreakGens) {
  //   std::cout << "max-cell-active-streak is higher than allowed by the
  //   hardcoded value!" << std::endl; exit(1);
  // }

  std::vector<LifeState> allSolutions;
  // std::vector<uint64_t> seenRotors;

  SearchState search(params, allSolutions);
  search.SearchStep();

  if (params.printSummary)
    PrintSummary(allSolutions);
}
