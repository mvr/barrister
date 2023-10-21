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

const unsigned maxLookaheadGens = 5;

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

  std::string RLE() const {
    LifeHistoryState history(state.state, state.unknown & ~state.unknownStable,
                             state.unknownStable, LifeState());
    return history.RLEWHeader();
  }
};

// struct FrontierCell {
//   std::pair<int, int> cell;
//   unsigned gen;
// };

struct Frontier {
  std::vector<FrontierGeneration> generations;
  // std::vector<FrontierCell> cells;
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
  std::pair<bool, FrontierGeneration> ResolveFrontierGeneration(LifeUnknownState &state, unsigned gen);
  std::pair<bool, Frontier> CalculateFrontier();
  bool UpdateFrontier();

  Transition AllowedTransitions(FrontierGeneration cellGeneration,
                                std::pair<int, int> frontierCell) const;
  StableOptions OptionsFor(LifeUnknownState state, std::pair<int, int> cell,
                           Transition transition) const;
  void ResolveFrontier();
  void SearchStep();

  void ReportSolution();
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

  // if (hasInteracted && !params->reportOscillators && gen > interactionStart +
  // params->maxActiveWindowGens && activePop > 0) {
  //   return ~LifeState();
  // }

  if (params->maxActiveCells != -1 &&
      activePop > (unsigned)params->maxActiveCells)
    return ~LifeState();

  LifeState result;

  if (params->maxActiveCells != -1 &&
      activePop == (unsigned)params->maxActiveCells)
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
  
  // No need to branch them separately
  if (((result & Transition::ON_TO_ON) == Transition::ON_TO_ON) &&
      !((result & Transition::OFF_TO_OFF) == Transition::OFF_TO_OFF))
    result &= ~Transition::STABLE_TO_STABLE;

  if (!((result & Transition::ON_TO_ON) == Transition::ON_TO_ON) &&
      ((result & Transition::OFF_TO_OFF) == Transition::OFF_TO_OFF))
    result &= ~Transition::STABLE_TO_STABLE;

  if ((result & Transition::STABLE_TO_STABLE) == Transition::STABLE_TO_STABLE)
    result &= ~(Transition::ON_TO_ON | Transition::OFF_TO_OFF);

  return result;
}

Transition
SearchState::AllowedTransitions(FrontierGeneration cellGeneration,
                                std::pair<int, int> frontierCell) const {
  // The frontier generation may be out of date with the stable state,
  // so we have to be a little careful

  bool prevState =
      cellGeneration.prev.state.Get(frontierCell) ||
      (cellGeneration.prev.unknownStable.Get(frontierCell) &&
       !stable.unknown.Get(frontierCell) && stable.state.Get(frontierCell));

  return ::AllowedTransitions(prevState, stable.unknown.Get(frontierCell),
                              stable.state.Get(frontierCell),
                              cellGeneration.forcedInactive.Get(frontierCell),
                              cellGeneration.forcedUnchanging.Get(frontierCell),
                              stable.stateZOI.Get(frontierCell),
                              cellGeneration.prev.TransitionFor(frontierCell));
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

  // std::cout << std::bitset<9>(stablemask) << std::endl;

  return StableOptionsForCounts(stablemask);
}

// Counts do not include the center
StableOptions OptionsFor(Transition transition, unsigned currenton,
                         unsigned unknown, unsigned stableon) {
  switch (transition) {
  case Transition::OFF_TO_OFF: return OptionsFor(false, false, currenton, unknown, stableon);
  case Transition::OFF_TO_ON:  return OptionsFor(false, true,  currenton, unknown, stableon);
  case Transition::ON_TO_OFF:  return OptionsFor(true,  false, currenton, unknown, stableon);
  case Transition::ON_TO_ON:   return OptionsFor(true,  true,  currenton, unknown, stableon);
  case Transition::STABLE_TO_STABLE:
    return (StableOptions::DEAD & OptionsFor(false, false, currenton, unknown, stableon)) |
           (StableOptions::LIVE & OptionsFor(true,  true,  currenton, unknown, stableon));
  }
}

StableOptions SearchState::OptionsFor(LifeUnknownState state,
                                      std::pair<int, int> cell,
                                      Transition transition) const {
  auto options = ::OptionsFor(transition, state.state.CountNeighbours(cell),
                              state.unknown.CountNeighbours(cell),
                              stable.state.CountNeighbours(cell));

  if (state.unknownStable.Get(cell)) {
    switch (transition)   {
    case Transition::OFF_TO_OFF:
    case Transition::OFF_TO_ON:
      options &= StableOptions::DEAD;
      break;
    case Transition::ON_TO_OFF:
    case Transition::ON_TO_ON:
      options &= StableOptions::LIVE;
      break;
    case Transition::STABLE_TO_STABLE:
      break;
    }
  }

  return options;
}


bool SearchState::UpdateActive(FrontierGeneration &generation) {
  generation.active = generation.state.ActiveComparedTo(stable);
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
  LifeState remainingCells = generation.frontierCells;

  bool someForced = false;

  for (auto cell = remainingCells.FirstOn(); cell != std::make_pair(-1, -1);
       remainingCells.Erase(cell), cell = remainingCells.FirstOn()) {
    auto allowedTransitions = AllowedTransitions(generation, cell);

    if (allowedTransitions == Transition::IMPOSSIBLE) {
      return {false, false};
    }

    // if (generation.forcedInactive.Get(cell) &&
    //     !TransitionIsSingleton(allowedTransitions)) {
    //   std::cout << generation.prev.unknown.Get(cell) << ", " << generation.prev.state.Get(cell) << std::endl;
    //   std::cout << generation.state.unknown.Get(cell) << ", " << generation.state.state.Get(cell) << std::endl;
    //   std::cout << stable.unknown.Get(cell) << ", " << stable.state.Get(cell) << std::endl;
    //   std::cout << std::bitset<5>(static_cast<unsigned char>(allowedTransitions)) << std::endl;
    // }

    // TODO: don't we possibly gain information even if it's not a singleton?
    if (TransitionIsSingleton(allowedTransitions)) {
      auto transition = allowedTransitions; // Just a rename

      // Resolve it immediately
      auto options = OptionsFor(generation.prev, cell, transition);
      stable.RestrictOptions(cell, options);

      if (stable.GetOptions(cell) == StableOptions::IMPOSSIBLE)
        return {false, false};

      stable.UpdateStateKnown(cell);

      generation.prev.SetTransitionPrev(cell, transition);
      generation.state.SetTransitionResult(cell, transition);

      someForced = true;
    }
  }

  return {true, someForced};
}

std::pair<bool, FrontierGeneration>
SearchState::ResolveFrontierGeneration(LifeUnknownState &state, unsigned gen) {
  // TODO: pass these in
  auto lookaheadTimer = activeTimer;
  auto lookaheadStreakTimer = streakTimer;

  FrontierGeneration frontierGeneration = {state, LifeUnknownState(), LifeState(), LifeState(), LifeState(), LifeState(), LifeState(), gen};

  bool done = false;
  while (!done) {
    frontierGeneration.state = frontierGeneration.prev.UncertainStepMaintaining(stable);

    bool updateresult = UpdateActive(frontierGeneration);
    if (!updateresult)
      return {false, frontierGeneration};

    // std::cout << "Resolving Before:" << std::endl;
    // std::cout << frontierGeneration.prev.ToHistory().RLEWHeader() << std::endl;
    // std::cout << "Resolving After:" << std::endl;
    // std::cout << frontierGeneration.state.ToHistory().RLEWHeader() << std::endl;

    LifeState prevUnknownActive = frontierGeneration.prev.unknown & ~frontierGeneration.prev.unknownStable;
    LifeState becomeUnknown = (frontierGeneration.state.unknown & ~frontierGeneration.state.unknownStable) & ~prevUnknownActive;

    frontierGeneration.frontierCells = becomeUnknown & ~prevUnknownActive.ZOI();
    LifeState remainingCells = frontierGeneration.frontierCells;

    auto [result, someForced] = SetForced(frontierGeneration);

    if (!result)
      return {false, frontierGeneration};

    if (someForced) {
      auto propagateResult = stable.Propagate();

      if (!propagateResult.consistent)
        return {false, frontierGeneration};

      stable.UpdateStateKnown();
      frontierGeneration.state.TransferStable(stable); // TODO: also transfer to earlier generations?
    } else {
      done = true;
    }
  }

  return {true, frontierGeneration};
}

std::pair<bool, Frontier> SearchState::CalculateFrontier() {
  Frontier frontier;

  // TODO: Maybe do a first pass to calculate ever-active? Active cells in a
  // later generation may constrain earlier generations.

  LifeUnknownState generation = current;

  for (unsigned i = 0; i < maxLookaheadGens; i++) {
    auto [consistent, resolved] = ResolveFrontierGeneration(generation, currentGen + i);

    if (!consistent)
      return {false, frontier};

    // if(i == 0) {
    // std::cout << "Before:" << std::endl;
    // std::cout << resolved.prev.ToHistory().RLEWHeader() << std::endl;
    // std::cout << "After:" << std::endl;
    // std::cout << LifeHistoryState(resolved.state.state, resolved.state.unknown, resolved.active).RLEWHeader() << std::endl;
    // }

    generation = resolved.state;

    if ((resolved.state.unknown & ~resolved.state.unknownStable).IsEmpty()) {
      // std::cout << "Advancing:" << std::endl;
      // std::cout << resolved.state.ToHistory().RLE() << std::endl;
      current = generation;
      currentGen += 1;
      i -= 1;

      // TODO: move these checks to their own function

      if (hasInteracted) {
        bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();

        if (isRecovered)
          recoveredTime++;
        else
          recoveredTime = 0;

        if (!isRecovered && currentGen > interactionStart + params->maxActiveWindowGens) {
          // TODO: This is the place to check for oscillators
          return {false, frontier};
        }

        if (isRecovered && recoveredTime == params->minStableInterval) {
          ReportSolution();
          return {false, frontier};
        }
      } else {
        if (currentGen > params->maxFirstActiveGen)
          return {false, frontier};
      }

      continue;
    }

    frontier.generations.push_back(resolved);

    // Continuing to advance won't find any frontier cells
    if (resolved.active.IsEmpty()) {
      break;
    }
  }
  return {true, frontier};
}

bool SearchState::UpdateFrontier() {
  bool anyChanges = false;
  for (auto &g : frontier.generations) {
    g.state.TransferStable(stable);

    auto [result, changes] = SetForced(g);
    if (!result)
      return false;
    anyChanges = anyChanges || changes;
  }

  if (anyChanges) {
    auto propagateResult = stable.Propagate();

    if (!propagateResult.consistent)
      return false;

    stable.UpdateStateKnown();
  }
  return true;
}

void SearchState::SearchStep() {
  // std::cout << "Stable Before Propagate:" << std::endl;
  // std::cout << stable.RLEWHeader() << std::endl;

  auto propagateResult = stable.Propagate();
  if (!propagateResult.consistent)
    return;

  // std::cout << "Stable After Propagate:" << std::endl;
  // std::cout << stable.RLEWHeader() << std::endl;

  // TODO: only transfer if necessary
  // if (propagateResult.changed)
  current.TransferStable(stable);


  bool allEmpty = true;
  for (auto &g : frontier.generations) {
    if (!g.frontierCells.IsEmpty()) {
      allEmpty = false;
      break;
    }
  }

  if (allEmpty) {
    bool consistent;
    std::tie(consistent, frontier) = CalculateFrontier();
    if (!consistent)
      return;

    // This is more important now than in old Barrister: we otherwise
    // spend a fair bit of time searching uncompletable parts of the
    // search space
    propagateResult = stable.TestUnknowns(stable.stateZOI & stable.unknown);
    // propagateResult = stable.TestUnknowns(stable.Vulnerable());
    if (!propagateResult.consistent)
      return;
    current.TransferStable(stable);
  } else {
    bool result = UpdateFrontier();
    if (!result)
      return;
  }

  std::pair<int, int> branchCell = {-1, -1};

  unsigned i;
  for (i = 0; i < frontier.generations.size(); i++) {
    // std::cout << "Choosing:" << std::endl;
    // std::cout << frontier.generations[i].state.ToHistory().RLEWHeader() << std::endl;
    branchCell = frontier.generations[i].frontierCells.FirstOn();
    if(branchCell.first != -1) break;
  }

  auto &frontierGeneration = frontier.generations[i];

  auto allowedTransitions = AllowedTransitions(frontierGeneration, branchCell);
  // std::cout << std::bitset<8>(static_cast<unsigned char>(allowedTransitions)) << std::endl;

  // Loop over the possible transitions
  for (auto t = TransitionHighest(allowedTransitions);
       t != Transition::IMPOSSIBLE;
       allowedTransitions &= ~t, t = TransitionHighest(allowedTransitions)) {

    // std::cout << "Branching:" << std::endl;
    // std::cout << std::bitset<5>(static_cast<unsigned char>(t)) << std::endl;

    auto currentoptions = stable.GetOptions(branchCell);
    auto newoptions = currentoptions & OptionsFor(frontierGeneration.prev, branchCell, t);

    // std::cout << std::bitset<8>(static_cast<unsigned char>(currentoptions)) << std::endl;
    // std::cout << std::bitset<8>(static_cast<unsigned char>(newoptions)) << std::endl;

    // std::cout << LifeHistoryState(frontierGeneration.prev.state, frontierGeneration.prev.unknown, LifeState::Cell(branchCell)).RLEWHeader() << std::endl;
    // std::cout << LifeHistoryState(frontierGeneration.state.state, frontierGeneration.state.unknown, LifeState::Cell(branchCell)).RLEWHeader() << std::endl;

    if (newoptions == StableOptions::IMPOSSIBLE)
      continue;

    SearchState newSearch = *this;
    newSearch.stable.RestrictOptions(branchCell, newoptions);
    newSearch.stable.UpdateStateKnown(branchCell);
    // TODO: StablePropagate the column of the cell now?

    newSearch.frontier.generations[i].frontierCells.Erase(branchCell);
    newSearch.frontier.generations[i].prev.SetTransitionPrev(branchCell, t);
    newSearch.frontier.generations[i].state.SetTransitionResult(branchCell, t);

    if (frontierGeneration.prev.TransitionIsPerturbation(branchCell, t)) {
      newSearch.stable.stateZOI.Set(branchCell);

      if (!hasInteracted) {
        // std::cout << "First Perturbing:" << std::endl;
        // std::cout << std::bitset<8>(static_cast<unsigned char>(t)) << std::endl;
        // std::cout << frontierGeneration.prev.ToHistory().RLEWHeader() << std::endl;
        // std::cout << LifeHistoryState(frontierGeneration.state.state, frontierGeneration.state.unknown, LifeState::Cell(branchCell)).RLEWHeader() << std::endl;
        // std::cout << branchCell.first << ", " << branchCell.second << std::endl;

        newSearch.hasInteracted = true;
        newSearch.interactionStart = i;
      }
    }

    newSearch.UpdateActive(newSearch.frontier.generations[i]);
    // TODO: we could modify `active` and `changed` directly, based on
    // the transition `t`, instead of recalculating them

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

  frontier = {std::vector<FrontierGeneration>()};

  everActive = LifeState();
  // activeTimer =
  // LifeCountdown<maxCellActiveWindowGens>(params->maxCellActiveWindowGens);
  // streakTimer =
  // LifeCountdown<maxCellActiveStreakGens>(params->maxCellActiveStreakGens);
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
