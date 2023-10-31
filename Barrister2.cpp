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

const unsigned maxBranchingGens = 3;
const unsigned maxFrontierGens = 10;
const unsigned maxLookaheadGens = 20;

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

  bool IsAlive() const {
    return !((prev.state ^ state.state) & ~prev.unknown & ~state.unknown).IsEmpty();
  }
  
  std::string RLE() const {
    LifeHistoryState history(state.state, state.unknown & ~state.unknownStable,
                             state.unknownStable, LifeState());
    return history.RLEWHeader();
  }
};

Transition AllowedTransitions(bool state, bool unknownstable, bool stablestate,
                              bool forcedInactive, bool forcedUnchanging, bool inzoi, Transition unperturbed) {
  auto result = Transition::ANY;

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
    result &= Transition::OFF_TO_OFF | Transition::ON_TO_ON | Transition::STABLE_TO_STABLE;

  return result;
}

Transition
AllowedTransitions(FrontierGeneration &generation,
                   const LifeStableState &stable,
                   std::pair<int, int> cell) {
  // The frontier generation may be out of date with the stable state,
  // so we have to be a little careful

  auto possibleTransitions = generation.prev.TransitionsFor(stable, cell);

  bool prevState =
      generation.prev.state.Get(cell) ||
      (generation.prev.unknownStable.Get(cell) &&
       !stable.unknown.Get(cell) && stable.state.Get(cell));

  auto allowedTransitions = ::AllowedTransitions(prevState, stable.unknown.Get(cell),
                              stable.state.Get(cell),
                              generation.forcedInactive.Get(cell),
                              generation.forcedUnchanging.Get(cell),
                              stable.stateZOI.Get(cell),
                              generation.prev.UnperturbedTransitionFor(cell));
  return TransitionSimplify(possibleTransitions & allowedTransitions);
}


struct Frontier {
  std::vector<FrontierGeneration> generations;
};

class SearchState {
public:
  LifeStableState stable;
  LifeUnknownState current;

  Frontier frontier;

  LifeState everActive;

  LifeCountdown<maxCellActiveWindowGens> activeTimer;
  LifeCountdown<maxCellActiveStreakGens> streakTimer;

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
      const LifeStableState &stable, const LifeUnknownState &previous,
      const LifeState &active, const LifeState &everActive,
      const LifeState &changes
      // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
                                ) const;
  LifeState ForcedUnchangingCells(
      unsigned gen, const LifeUnknownState &state,
      const LifeStableState &stable, const LifeUnknownState &previous,
      const LifeState &active, const LifeState &everActive,
      const LifeState &changes
      // const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      // const LifeCountdown<maxCellActiveStreakGens> &streakTimer
          ) const;

  bool UpdateActive(FrontierGeneration &generation);
  std::pair<bool, bool> SetForced(FrontierGeneration &generation);

  std::tuple<bool, bool, Frontier> PopulateFrontier();
  std::pair<bool, bool> RefineFrontierStep();
  std::pair<bool, bool> RefineFrontier();
  std::pair<bool, Frontier> CalculateFrontier();
  std::pair<bool, bool> TryAdvance();

  Transition AllowedTransitions(FrontierGeneration &cellGeneration,
                                std::pair<int, int> frontierCell) const;
  StableOptions OptionsFor(const LifeUnknownState &state,
                           std::pair<int, int> cell,
                           Transition transition) const;
  void ResolveFrontier();

  bool FrontierComplete() const;
  std::pair<unsigned, std::pair<int, int>> ChooseBranchCell() const;

  void SearchStep();

  void ReportSolution();

  LifeState FrontierCells() const;
  void SanityCheck();
};

LifeState SearchState::ForcedInactiveCells(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeUnknownState &previous, const LifeState &active,
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

  return result;
}
LifeState SearchState::ForcedUnchangingCells(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeUnknownState &previous, const LifeState &active,
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

  return StableOptionsForCounts(stablemask);
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
  generation.changes = generation.state.ChangesComparedTo(generation.state) & stable.stateZOI;

  generation.forcedInactive = ForcedInactiveCells(
      generation.gen, generation.state, stable, generation.prev,
      generation.active, everActive, generation.changes);

  if (!(generation.active & generation.forcedInactive).IsEmpty()) {
    return false;
  }

  generation.forcedUnchanging = ForcedUnchangingCells(
      generation.gen, generation.state, stable, generation.prev,
      generation.active, everActive, generation.changes);

  if (!(generation.changes & generation.forcedUnchanging).IsEmpty()) {
    return false;
  }

  return true;
}

std::pair<bool, bool> SearchState::SetForced(FrontierGeneration &generation) {
  bool someForced = false;

  stable.SanityCheck();
  LifeState remainingCells = generation.frontierCells;
  for (auto cell = remainingCells.FirstOn(); cell != std::make_pair(-1, -1);
       remainingCells.Erase(cell), cell = remainingCells.FirstOn()) {

    // Check whether the stable state of the cell is actually forced
    if(stable.unknown.Get(cell)) {
      auto propagateResult = stable.TestUnknown(cell);

      if (!propagateResult.consistent)
        return {false, false};

      if (propagateResult.changed) {
        // TODO: just the strip
        stable.SynchroniseStateKnown();
        generation.prev.TransferStable(stable);
        generation.state.TransferStable(stable);
        someForced = true;
      }
    }

    // Check whether the transition was already figured out by other means
    if ((!generation.prev.unknown.Get(cell) && !generation.state.unknown.Get(cell)) || generation.state.unknownStable.Get(cell)) {
      generation.frontierCells.Erase(cell);
      continue;
    }

    auto allowedTransitions = ::AllowedTransitions(generation, stable, cell);

    if (allowedTransitions == Transition::IMPOSSIBLE) {
      return {false, false};
    }

    // TODO: don't we possibly gain information even if it's not a singleton?
    if (TransitionIsSingleton(allowedTransitions)) {
      auto transition = allowedTransitions; // Just a rename

      // if(transition == Transition::STABLE_TO_STABLE) {
      //   auto propagateResult = stable.TestUnknowns(LifeState::Cell(cell));
      //   if (!propagateResult.consistent)
      //     return {false, false};
      // }
      auto startingOptions = stable.GetOptions(cell);
      auto options = OptionsFor(generation.prev, cell, transition);
      stable.RestrictOptions(cell, options);
      stable.SynchroniseStateKnown(cell);
      auto newOptions = stable.GetOptions(cell);

      if (newOptions == StableOptions::IMPOSSIBLE)
        return {false, false};

      // Correct the transition, if we have actually learned the stable state
      if (transition == Transition::STABLE_TO_STABLE && !stable.unknown.Get(cell)) {
        someForced = true;
        if (stable.state.Get(cell))
          transition = Transition::ON_TO_ON;
        else
          transition = Transition::OFF_TO_OFF;
      }

      generation.SetTransition(cell, transition);

      generation.frontierCells.Erase(cell);

      bool alreadyFixed = startingOptions == newOptions;
      if(!alreadyFixed) {
        someForced = true;
      }

      stable.SanityCheck();
      generation.prev.SanityCheck(stable);
      generation.state.SanityCheck(stable);
    }
  }

  return {true, someForced};
}

std::tuple<bool, bool, Frontier> SearchState::PopulateFrontier() {
  bool anyForced = false;

  auto [result, someForced] = stable.StabiliseOptions();
  if (!result)
     return {false, false, frontier};
  current.TransferStable(stable);
  anyForced = anyForced || someForced;

  Frontier frontier;

  LifeUnknownState generation = current;
  unsigned gen = currentGen;

  for (unsigned i = 0; i < maxFrontierGens; i++) {
    gen++;


    FrontierGeneration frontierGeneration = {
        generation,  generation.StepMaintaining(stable),
        LifeState(), LifeState(),
        LifeState(), LifeState(),
        LifeState(), gen};

    stable.SanityCheck();
    frontierGeneration.prev.SanityCheck(stable);
    frontierGeneration.state.SanityCheck(stable);

    bool updateresult = UpdateActive(frontierGeneration);
    if (!updateresult)
      return {false, false, frontier};

    LifeState prevUnknownActive = frontierGeneration.prev.unknown & ~frontierGeneration.prev.unknownStable;
    LifeState becomeUnknown = (frontierGeneration.state.unknown & ~frontierGeneration.state.unknownStable) & ~prevUnknownActive;

    frontierGeneration.frontierCells = becomeUnknown & ~prevUnknownActive.ZOI();


    frontierGeneration.state.SanityCheck(stable);

    if(!frontierGeneration.frontierCells.IsEmpty())
      frontier.generations.push_back(frontierGeneration);

    if (!frontierGeneration.IsAlive())
      break;

    generation = frontierGeneration.state;
  }
  return {true, anyForced, frontier};
}

std::pair<bool, bool> SearchState::RefineFrontierStep() {
  bool anyChanges = false;
  for (auto &g : frontier.generations) {
    g.prev.TransferStable(stable);
    g.state.TransferStable(stable);

    bool updateresult = UpdateActive(g);
    if (!updateresult)
      return {false, false};

    auto [result, changes] = SetForced(g);
    if (!result)
      return {false, false};

    anyChanges = anyChanges || changes;
  }

  if (anyChanges) {
    auto propagateResult = stable.Propagate();
    anyChanges = anyChanges || propagateResult.changed;

    if (!propagateResult.consistent)
      return {false, false};
  }

  return {true, anyChanges};
}

std::pair<bool, bool> SearchState::RefineFrontier() {
  bool done = false;
  bool anyChanges = false;
  while (!done) {
    auto [consistent, changed] = RefineFrontierStep();
    if (!consistent)
      return {false, false};

    anyChanges = anyChanges || changed;
    done = !changed;
  }
  return {true, anyChanges};
}

std::pair<bool, bool> SearchState::TryAdvance() {
  current.TransferStable(stable);

  bool didAdvance = false;
  while (true) {
    auto next = current.StepMaintaining(stable);

    if (!(next.unknown & ~next.unknownStable).IsEmpty())
      break;

    didAdvance = true;

    current = next;
    currentGen += 1;

    if (frontier.generations.size() > 0 && frontier.generations[0].gen == currentGen) {
      frontier.generations.erase(frontier.generations.begin());
    }

    if (hasInteracted) {
      bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();

      if (isRecovered)
        recoveredTime++;
      else
        recoveredTime = 0;

      if (!isRecovered &&
          currentGen > interactionStart + params->maxActiveWindowGens) {
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

std::pair<bool, Frontier> SearchState::CalculateFrontier() {
  bool consistent;

  bool anyChanges = true;
  while (anyChanges) {
    anyChanges = false;
    bool someChanges;
    std::tie(consistent, someChanges, frontier) = PopulateFrontier();
    if (!consistent)
      return {false, frontier};
    anyChanges = anyChanges || someChanges;

    // Fold into PopulateFrontier?
    auto propagateResult = stable.Propagate();
    anyChanges = anyChanges || propagateResult.changed;

    if (!propagateResult.consistent)
      return {false, frontier};

    std::tie(consistent, someChanges) = RefineFrontier();
    if (!consistent)
      return {false, frontier};
    anyChanges = anyChanges || someChanges;
  }

  bool didAdvance = false;
  std::tie(consistent, didAdvance) = TryAdvance();
  if (!consistent)
    return {false, frontier};
  if (didAdvance) {
    if (frontier.generations.size() == 0) {
      // We have to start over
      return CalculateFrontier();
    }
  }

#ifdef DEBUG
  for (auto &generation : frontier.generations) {
    LifeState remainingCells = generation.frontierCells;
    for (auto cell = remainingCells.FirstOn(); cell != std::make_pair(-1, -1);
         remainingCells.Erase(cell), cell = remainingCells.FirstOn()) {
      auto allowedTransitions = ::AllowedTransitions(generation, stable, cell);
      assert(!TransitionIsSingleton(allowedTransitions));
    }
  }
#endif

  return {true, frontier};
}

bool SearchState::FrontierComplete() const {
  bool isEmpty = true;
  for (auto &g : frontier.generations) {
    if (g.gen > currentGen + maxBranchingGens)
      break;
    if (!g.frontierCells.IsEmpty()) {
      isEmpty = false;
      break;
    }
  }
  return isEmpty;
}

std::pair<unsigned, std::pair<int, int>> SearchState::ChooseBranchCell() const {
  // Prefer unknown stable cells?
  // Prefer lower numbers of possible transitions?
  // Prefer cells where all options are active?
  unsigned i;
  std::pair<int, int> branchCell = {-1, -1};
  // for (unsigned i = 0; i < generations.size(); i++) {
  //   std::pair<int, int> branchCell = generations[i].frontierCells.FirstOn();
  //   if (branchCell.first != -1) {
  //     auto allowedTransitions = ::AllowedTransitions(generations[i], stable, branchCell);
  //     if ((allowedTransitions & Transition::STABLE_TO_STABLE) != Transition::STABLE_TO_STABLE)
  //     // if(TransitionCount(allowedTransitions) <= 3)
  //       return {i, branchCell};
  //   }
  // }
  for (i = 0; i < frontier.generations.size(); i++) {
    branchCell = frontier.generations[i].frontierCells.FirstOn();
    if(branchCell.first != -1) break;
  }

  return {i, branchCell};
}

void SearchState::SearchStep() {
  // Don't totally recalculate unless necessary
  auto [consistent, someChanges] = RefineFrontier();
  if (!consistent)
    return;

  if (FrontierComplete()) {
    bool consistent;
    std::tie(consistent, frontier) = CalculateFrontier();
    if (!consistent)
      return;
  }

  SanityCheck();

  // std::cout << "Stable:" << std::endl;
  // std::cout << stable.RLEWHeader() << std::endl;
  // std::cout << "Frontier:" << std::endl;
  // std::cout << "Gen: " << currentGen << std::endl;
  // for (auto &g : frontier.generations) {
  //   std::cout << g.state.ToHistory().RLEWHeader() << std::endl;
  // }

  auto [i, branchCell] = ChooseBranchCell();
  auto &frontierGeneration = frontier.generations[i];

  assert(branchCell.first != -1);

  auto allowedTransitions = ::AllowedTransitions(frontierGeneration, stable, branchCell);
  // The cell should not still be in the frontier in this case
  assert(!TransitionIsSingleton(allowedTransitions));

  // Loop over the possible transitions
  for (auto transition = TransitionHighest(allowedTransitions);
       transition != Transition::IMPOSSIBLE;
       allowedTransitions &= ~transition, transition = TransitionHighest(allowedTransitions)) {

    auto newoptions = stable.GetOptions(branchCell) & OptionsFor(frontierGeneration.prev, branchCell, transition);

    if (newoptions == StableOptions::IMPOSSIBLE)
      continue;

    SearchState newSearch = *this;
    newSearch.stable.RestrictOptions(branchCell, newoptions);
    newSearch.stable.SynchroniseStateKnown(branchCell);

    newSearch.frontier.generations[i].frontierCells.Erase(branchCell);
    newSearch.frontier.generations[i].SetTransition(branchCell, transition);

    if (frontierGeneration.prev.TransitionIsPerturbation(branchCell, transition)) {
      newSearch.stable.stateZOI.Set(branchCell);

      if (!hasInteracted) {
        newSearch.hasInteracted = true;
        newSearch.interactionStart = frontierGeneration.gen;
      }
    }

    newSearch.current.TransferStable(newSearch.stable);

    newSearch.SearchStep();
  }
}

SearchState::SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions) : currentGen{0}, hasInteracted{false}, interactionStart{0} {
  params = &inparams;
  allSolutions = &outsolutions;

  stable.state = inparams.startingStable;
  stable.unknown = inparams.searchArea;

  // This needs to be done in this order first, because the counts/options start at all 0
  stable.UpdateOptions();
  stable.Propagate();

  current.state = inparams.startingPattern;
  current.unknown = stable.unknown;
  current.unknownStable = stable.unknown;

  TryAdvance();

  frontier = {std::vector<FrontierGeneration>()};

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

LifeState SearchState::FrontierCells() const {
  LifeState result;
  for (auto &g : frontier.generations) {
    result |= g.frontierCells;
  }
  return result;
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
