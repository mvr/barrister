#include <cassert>
#include <stack>

#include "toml/toml.hpp"

#include "LifeAPI.h"
#include "LifeStableState.hpp"
#include "LifeUnknownState.hpp"
#include "Params.hpp"

const unsigned maxLookaheadGens = 3;
const unsigned maxLightspeedDistance = maxLookaheadGens;
const unsigned maxLookaheadKnownPop = 16;
static_assert(maxLookaheadKnownPop > maxLookaheadGens);
const unsigned maxCellActiveWindowGens = 8 - 1;
const unsigned maxCellActiveStreakGens = 8 - 1;

struct FocusSet {
  LifeState focuses;
  LifeState nonGlancingFocuses;
  LifeState glanceable;

  LifeUnknownState currentState;
  unsigned currentGen;
  bool isForcedInactive;

  bool hasNonGlancing;

  FocusSet() = default;

  FocusSet(LifeState &infocuses, LifeState &inglanceable, LifeUnknownState &incurrentState, unsigned incurrentGen, bool inisForcedInactive) {
    focuses = infocuses;
    glanceable = inglanceable;
    currentState = incurrentState;
    currentGen = incurrentGen;
    isForcedInactive = inisForcedInactive;

    nonGlancingFocuses = focuses & ~glanceable;
    hasNonGlancing = !nonGlancingFocuses.IsEmpty();
  }

  std::pair<int, int> NextFocus()  {
    std::pair<int, int> focus;

    if(hasNonGlancing) {
      focus = nonGlancingFocuses.FirstOn();
      if (focus != std::pair(-1, -1))
        return focus;
    }
    hasNonGlancing = false;

    return focuses.FirstOn();
  }

  void Erase(std::pair<int, int> cell) {
    focuses.Erase(cell);
    nonGlancingFocuses.Erase(cell);
  }
};


class SearchState {
public:

  LifeStableState stable;
  LifeUnknownState current;

  LifeState everActive;

  FocusSet pendingFocuses;

  // Monotonically increasing as cells are set, until a step is taken.
  std::array<uint16_t, maxLookaheadKnownPop> lookaheadKnownPop;

  LifeCountdown<maxCellActiveWindowGens> activeTimer;
  LifeCountdown<maxCellActiveStreakGens> streakTimer;

  std::pair<int, int> focus;

  unsigned currentGen;
  bool hasInteracted;
  bool hasReported;
  unsigned interactionStart;
  unsigned recoveredTime;

  SearchParams *params;
  std::vector<LifeState> *allSolutions;
  std::vector<uint64_t> *seenRotors;

  SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions, std::vector<uint64_t> &outrotors);
  SearchState ( const SearchState & ) = default;
  SearchState &operator= ( const SearchState & ) = default;

  void TransferStableToCurrent();
  void TransferStableToCurrentColumn(unsigned column);
  bool TryAdvance();
  bool TestRecovered();
  unsigned TestOscillating();
  std::vector<uint64_t> ClassifyRotors(unsigned period);

  std::pair<bool, FocusSet> FindFocuses();

  bool CheckConditionsOn(
      unsigned gen, const LifeUnknownState &state, const LifeStableState &stable, const LifeUnknownState &previous, const LifeState &active,
      const LifeState &everActive,
      const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      const LifeCountdown<maxCellActiveStreakGens> &streakTimer) const;
  LifeState ForcedInactiveCells(
      unsigned gen, const LifeUnknownState &state,
      const LifeStableState &stable, const LifeUnknownState &previous,
      const LifeState &active, const LifeState &everActive,
      const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
      const LifeCountdown<maxCellActiveStreakGens> &streakTimer) const;

  void Search();
  void SearchStep();

  bool ContainsEater2(LifeState &stable, LifeState &everActive) const;
  bool PassesFilter() const;
  void ReportSolution();
  void ReportFullSolution();
  void ReportPipeSolution();

  void SanityCheck();
};

// std::string SearchState::LifeBellmanRLE() const {
//   LifeState state = stable | params.activePattern;
//   LifeState marked =  unknown | stable;
//   return LifeBellmanRLEFor(state, marked);
// }

SearchState::SearchState(SearchParams &inparams, std::vector<LifeState> &outsolutions, std::vector<uint64_t> &outrotors)
  : currentGen{0}, hasInteracted{false}, hasReported{false}, interactionStart{0}, recoveredTime{0} {

  params = &inparams;
  allSolutions = &outsolutions;
  seenRotors = &outrotors;

  stable.state = inparams.startingStable;
  stable.unknownStable = inparams.searchArea;

  current.state = inparams.startingPattern;
  current.unknown = stable.unknownStable;
  current.unknownStable = stable.unknownStable;

  everActive = LifeState();
  lookaheadKnownPop = {0};
  focus = {-1, -1};
  pendingFocuses.focuses = LifeState();
  activeTimer = LifeCountdown<maxCellActiveWindowGens>(params->maxCellActiveWindowGens);
  streakTimer = LifeCountdown<maxCellActiveStreakGens>(params->maxCellActiveStreakGens);
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

bool SearchState::CheckConditionsOn(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeUnknownState &previous, const LifeState &active,
    const LifeState &everActive,
    const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
    const LifeCountdown<maxCellActiveStreakGens> &streakTimer) const {
  auto activePop = active.GetPop();

  if (gen < params->minFirstActiveGen && activePop > 0)
    return false;

  if (params->maxActiveCells != -1 && activePop > (unsigned)params->maxActiveCells)
    return false;

  if (params->maxChanges != -1 || params->changesBounds.first != -1) {
    LifeState changes = (state.state ^ previous.state) & ~state.unknown & stable.stateZOI;
    if (params->maxChanges != -1) {
      if (changes.GetPop() > params->maxChanges)
        return false;
    }

    if (params->changesBounds.first != -1) {
      auto wh = changes.WidthHeight();
      if (wh.first > params->changesBounds.first || wh.second > params->changesBounds.second)
        return false;
    }
  }

  if(hasInteracted && !params->reportOscillators && gen > interactionStart + params->maxActiveWindowGens && activePop > 0)
    return false;

  if (params->maxCellActiveWindowGens != -1 && currentGen > (unsigned)params->maxCellActiveWindowGens && !(active & activeTimer.finished).IsEmpty())
    return false;

  if (params->maxCellActiveStreakGens != -1 && currentGen > (unsigned)params->maxCellActiveStreakGens && !(active & streakTimer.finished).IsEmpty())
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

  if (params->filterGen != -1 && gen == (unsigned)params->filterGen) {
    if (!((state.state ^ params->filterPattern) & params->filterMask &
          ~state.unknown)
             .IsEmpty()) {
      return false;
    }
  }


  return true;
}

// Cells that must be inactive or CheckConditions will fail
// So, it should be that CheckConditionsOn == !(ForcedInactiveCells &
// active).IsEmpty()
LifeState SearchState::ForcedInactiveCells(
    unsigned gen, const LifeUnknownState &state, const LifeStableState &stable,
    const LifeUnknownState &previous, const LifeState &active,
    const LifeState &everActive,
    const LifeCountdown<maxCellActiveWindowGens> &activeTimer,
    const LifeCountdown<maxCellActiveStreakGens> &streakTimer) const {
  if (gen < params->minFirstActiveGen) {
    return ~LifeState();
  }

  auto activePop = active.GetPop();

  if (hasInteracted && !params->reportOscillators && gen > interactionStart + params->maxActiveWindowGens && activePop > 0) {
    return ~LifeState();
  }

  if (params->maxActiveCells != -1 &&
      activePop > (unsigned)params->maxActiveCells)
    return ~LifeState();

  LifeState result;

  if (params->maxActiveCells != -1 &&
      activePop == (unsigned)params->maxActiveCells)
    result |= ~active; // Or maybe just return

  if (params->maxChanges != -1 || params->changesBounds.first != -1) {
    LifeState changes = (state.state ^ previous.state) & ~state.unknown & stable.stateZOI;
    if (params->maxChanges != -1) {
      unsigned changesPop = changes.GetPop();
      if (changesPop > params->maxChanges)
        return ~LifeState();
      if (changesPop == params->maxChanges) {
        LifeState prevactive = previous.ActiveComparedTo(stable);
        result |= ~prevactive & ~active & ~changes;
      }
    }

    if (params->changesBounds.first != -1) {
      LifeState prevactive = previous.ActiveComparedTo(stable);
      result |= ~prevactive & ~active & ~changes.BufferAround(params->changesBounds);
    }
  }

  if (params->maxCellActiveWindowGens != -1 &&
      currentGen > (unsigned)params->maxCellActiveWindowGens)
    result |= activeTimer.finished;

  if (params->maxCellActiveStreakGens != -1 &&
      currentGen > (unsigned)params->maxCellActiveStreakGens)
    result |= streakTimer.finished;

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

  if (params->hasStator)
    result |= params->stator;

  return result;
}

bool SearchState::TryAdvance() {
  while (true) {
    LifeUnknownState next = current.UncertainStepMaintaining(stable);
    bool fullyKnown = (next.unknown ^ next.unknownStable).IsEmpty();

    if (!fullyKnown)
      break;

    // Test whether we interact now
    if(!hasInteracted) {
      LifeState steppedWithoutStable = (current.state & ~stable.state);
      steppedWithoutStable.Step();

      bool isDifferent = !(next.state ^ (steppedWithoutStable | stable.state)).IsEmpty();

      if (isDifferent) {
        // Too early:
        if (currentGen < params->minFirstActiveGen)
          return false;

        hasInteracted = true;
        interactionStart = currentGen;
      } else {
        // Too late:
        if(currentGen > params->maxFirstActiveGen)
          return false;
      }
    }

    LifeUnknownState previous = current;
    current = next;
    currentGen++;
    lookaheadKnownPop = {0};

    // Test recovery
    if (hasInteracted) {
      bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();

      if (!hasReported && isRecovered && recoveredTime == 0) {
        // See whether this is already a solution with no additional ON cells

        // TODO: This could definitely be done without copying the
        // entire state, but I am too lazy.
        SearchState testState = *this;

        bool succeeded = testState.TestRecovered();
        if (succeeded) {
          if(!params->reportOscillators)
            testState.ReportSolution();
          hasReported = true;
          if(!params->continueAfterSuccess)
            return false;
        }
      }

      if (isRecovered)
        recoveredTime++;
      else
        recoveredTime = 0;

      if (currentGen > interactionStart + params->maxActiveWindowGens) {
        if(params->reportOscillators) {
          unsigned period = TestOscillating();
          if (period > 3) {
            auto rotors = ClassifyRotors(period);
            bool anyNew = false;
            for(uint64_t r : rotors) {
              if(std::find(seenRotors->begin(), seenRotors->end(), r) == seenRotors->end()) {
                anyNew = true;
                seenRotors->push_back(r);
              }
            }
            if(anyNew) {
              std::cout << "Oscillating! Period: " << period << std::endl;
              ReportSolution();
            }
          }
        }

        return false;
      }
    }

    LifeState active = current.ActiveComparedTo(stable);
    everActive |= active;

    if (params->maxCellActiveWindowGens != -1) {
      activeTimer.Start(active);
      activeTimer.Tick();
    }

    if (params->maxCellActiveStreakGens != -1) {
      streakTimer.Reset(~active);
      streakTimer.Start(active);
      streakTimer.Tick();
    }

    if (!CheckConditionsOn(currentGen, current, stable, previous, active, everActive, activeTimer, streakTimer))
      return false;
  }

  return true;
}

// See whether the current stable is a successful catalyst on its own
bool SearchState::TestRecovered() {
  for (unsigned i = 1; i < params->minStableInterval; i++) {
    LifeState active = stable.state ^ current.state;
    LifeState toClear = active.ZOI().MooreZOI() & stable.unknownStable;

    current.unknown &= ~toClear;
    current.unknownStable &= ~toClear;
    stable.unknownStable &= ~toClear;
    CountNeighbourhood(stable.unknownStable, stable.unknown3, stable.unknown2, stable.unknown1, stable.unknown0);

    LifeUnknownState next = current.UncertainStepMaintaining(stable);
    current = next;

    bool isRecovered = ((stable.state ^ current.state) & stable.stateZOI).IsEmpty();
    if (!isRecovered)
      return false;
  }
  return true;
}

unsigned SearchState::TestOscillating() {
  // We can trample `current`, because we only do this when we are
  // going to bail out of the branch anyway.

  std::stack<std::pair<uint64_t, int>> minhashes;

  // TODO: is 60 reasonable?
  for (unsigned i = 1; i < 60; i++) {
    LifeState active = stable.state ^ current.state;

    uint64_t newhash = active.GetHash();

    while(true) {
      if(minhashes.empty())
        break;
      if(minhashes.top().first < newhash)
        break;

      if(minhashes.top().first == newhash) {
        unsigned p = i - minhashes.top().second;
        return p;
      }

      if(minhashes.top().first > newhash)
        minhashes.pop();
    }

    minhashes.push({newhash, i});

    current = current.UncertainStepMaintaining(stable);
  }
  return 0;
}

std::vector<uint64_t> SearchState::ClassifyRotors(unsigned period) {
  // We shouldn't use the stable state in here, because at this point
  // we don't care what the original background of the rotor was
  LifeState startState = current.state;

  LifeState allRotorCells;
  for(unsigned i = 0; i < period; i++) {
    LifeState active = startState ^ current.state;
    allRotorCells |= active;
    current = current.UncertainStepMaintaining(stable);
  }

  std::vector<uint64_t> result;

  auto rotorLocations = allRotorCells.Components();
  for(auto &rotorLocation : rotorLocations) {
    LifeState rotorZOI = rotorLocation.ZOI();
    LifeState rotorStart = current.state & rotorZOI;

    uint64_t rotorHash = 0;
    for(unsigned i = 0; i < period; i++) {
      LifeState rotorState = rotorZOI & ~(current.state & allRotorCells);
      rotorHash ^= rotorState.GetOctoHash();
      current = current.UncertainStepMaintaining(stable);
      if((current.state & rotorZOI) == rotorStart)
        break;
    }
    result.push_back(rotorHash);
  }
  return result;
}

std::pair<bool, FocusSet> SearchState::FindFocuses() {
  auto lookahead = std::array<LifeUnknownState, maxLookaheadGens>();
  unsigned lookaheadSize;

  std::array<LifeState, maxLookaheadGens> allFocusable;
  std::array<bool, maxLookaheadGens> genHasFocusable;
  std::array<LifeState, maxLookaheadGens> allForcedInactive;
  auto lookaheadTimer = activeTimer;
  auto lookaheadStreakTimer = streakTimer;

  lookahead[0] = current;
  unsigned i;
  for (i = 1; i < maxLookaheadGens; i++) {
    lookahead[i] = lookahead[i - 1].UncertainStepMaintaining(stable);
    lookaheadSize = i + 1;
    LifeUnknownState &gen = lookahead[i];
    LifeUnknownState &prev = lookahead[i-1];

    LifeState active = gen.ActiveComparedTo(stable);

    everActive |= active;
    if (params->maxCellActiveWindowGens != -1) {
      lookaheadTimer.Start(active);
      lookaheadTimer.Tick();
    }
    if (params->maxCellActiveStreakGens != -1) {
      lookaheadStreakTimer.Reset(~active);
      lookaheadStreakTimer.Start(active);
      lookaheadStreakTimer.Tick();
    }

    allForcedInactive[i] = ForcedInactiveCells(currentGen + i, gen, stable, prev, active, everActive, lookaheadTimer, lookaheadStreakTimer);

    if(!(allForcedInactive[i] & active).IsEmpty())
      return {false, FocusSet()};

    LifeState becomeUnknown = (gen.unknown & ~gen.unknownStable) & ~(prev.unknown & ~prev.unknownStable);
    LifeState nearActiveUnknown = (prev.unknown & ~prev.unknownStable).ZOI();

    allFocusable[i] = becomeUnknown & ~nearActiveUnknown;
    genHasFocusable[i] = !allFocusable[i].IsEmpty();

    unsigned knownPop = (~gen.unknown & ~stable.unknownStable & stable.stateZOI).GetPop();
    if (currentGen + i > pendingFocuses.currentGen && knownPop == lookaheadKnownPop[i])
      break;
    lookaheadKnownPop[i] = knownPop;
  }

  // Continue the lookahead until we run out of active cells
  if (hasInteracted && lookaheadSize == maxLookaheadGens) {
    LifeUnknownState gen = lookahead[maxLookaheadGens - 1];
    for(unsigned i = maxLookaheadGens; currentGen + i <= interactionStart + params->maxActiveWindowGens + 1; i++) {
      LifeUnknownState prev = gen;
      gen = gen.UncertainStepFast(stable);
      LifeState active = gen.ActiveComparedTo(stable);

      if (i < maxLookaheadKnownPop) {
        unsigned knownPop = (~gen.unknown & ~stable.unknownStable & stable.stateZOI).GetPop();
        if (knownPop == lookaheadKnownPop[i])
          break;
        lookaheadKnownPop[i] = knownPop;
      } else {
        if(active.IsEmpty())
          break;
      }

      everActive |= active;

      if (params->maxCellActiveWindowGens != -1) {
        lookaheadTimer.Start(active);
        lookaheadTimer.Tick();
      }
      if (params->maxCellActiveStreakGens != -1) {
        lookaheadStreakTimer.Reset(~active);
        lookaheadStreakTimer.Start(active);
        lookaheadStreakTimer.Tick();
      }

      bool genResult = CheckConditionsOn(currentGen + i, gen, stable, prev, active, everActive, lookaheadTimer, lookaheadStreakTimer);
      if (!genResult)
        return {false, FocusSet()};
    }
  }

  LifeState oneOrTwoUnknownNeighbours = (stable.unknown0 ^ stable.unknown1) & ~stable.unknown2 & ~stable.unknown3;

  int bestPrioGen = -1;
  int bestPrioDistance = -1;
  LifeState bestPrioCandidates(false);

  int bestEdgyGen = -1;
  LifeState bestEdgyCandidates(false);

  int bestAnyGen = -1;
  LifeState bestAnyCandidates(false);

  for (unsigned l = 0; l < maxLightspeedDistance; l++) {
    for (unsigned i = 1; i + l < lookaheadSize; i++) {
      if (!genHasFocusable[i])
        continue;
      LifeState prioCandidates = allForcedInactive[i+l] & allFocusable[i];
      LifeState edgyPrioCandidates = oneOrTwoUnknownNeighbours & prioCandidates;

      if (!edgyPrioCandidates.IsEmpty()) {
        return {true, FocusSet(edgyPrioCandidates, lookahead[i].glanceableUnknown, lookahead[i - 1], currentGen + i - 1, l == 0)};
      }

      if (bestPrioGen == -1 && !prioCandidates.IsEmpty()) {
        bestPrioGen = i;
        bestPrioDistance = l;
        bestPrioCandidates = prioCandidates;
      }

      LifeState edgyCandidates = allFocusable[i] & oneOrTwoUnknownNeighbours;

      if (l == 0 && bestEdgyGen == -1 && !edgyCandidates.IsEmpty()) {
        bestEdgyGen = i;
        bestEdgyCandidates = edgyCandidates;
      }

      if (l == 0 && bestAnyGen == -1 && !allFocusable[i].IsEmpty()) {
        bestAnyGen = i;
        bestAnyCandidates = allFocusable[i];
      }
    }

    for (unsigned i = 1; i + l < lookaheadSize; i++) {
      allForcedInactive[i] = allForcedInactive[i].ZOI();
    }
  }

  if (bestPrioGen != -1) {
    int i = bestPrioGen;
    return {true, FocusSet(bestPrioCandidates, lookahead[i].glanceableUnknown, lookahead[i - 1], currentGen + i - 1, bestPrioDistance == 0)};
  }

  if (bestEdgyGen != -1) {
    int i = bestEdgyGen;
    return {true, FocusSet(bestEdgyCandidates, lookahead[i].glanceableUnknown, lookahead[i - 1], currentGen + i - 1, false)};
  }

  if (bestAnyGen != -1) {
    int i = bestAnyGen;
    return {true, FocusSet(bestAnyCandidates, lookahead[i].glanceableUnknown, lookahead[i - 1], currentGen + i - 1, false)};
  }

  // This shouldn't be reached
  return {false, FocusSet()};
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

    LifeState cells = stable.Vulnerable() & stable.unknownStable;
    bool testconsistent = stable.TestUnknowns(cells);
    if (!testconsistent)
      return;
    TransferStableToCurrent();

    if (!TryAdvance())
      return;

    bool passed;
    std::tie(passed, pendingFocuses) = FindFocuses();

    if (!passed)
      return;

    // SanityCheck();
  }

  if (focus == std::pair(-1, -1)) {
    focus = pendingFocuses.NextFocus();
    if (focus == std::pair(-1, -1)) {
      std::cout << "no focus" << std::endl;
      exit(1);
    }

    bool focusIsGlancing =
        params->skipGlancing && pendingFocuses.glanceable.Get(focus) &&
        pendingFocuses.currentState.StillGlancingFor(focus, stable);

    if(focusIsGlancing) {
      pendingFocuses.Erase(focus);

      if (!pendingFocuses.isForcedInactive || stable.unknown2.Get(focus) ||
          stable.unknown3.Get(focus)) { // TODO: handle overpopulation better

        SearchState nextState = *this;
        nextState.stable.glancedON.Set(focus);
        nextState.SearchStep();
      }

      stable.glanced.Set(focus);
      pendingFocuses.Erase(focus);
      focus = {-1, -1};

      [[clang::musttail]]
      return SearchStep();
    }
  }

  bool focusIsDetermined = pendingFocuses.currentState.KnownNext(focus);

  auto cell = stable.UnknownNeighbour(focus);
  if (focusIsDetermined || cell == std::pair(-1, -1)) {
    pendingFocuses.Erase(focus);
    focus = {-1, -1};

    [[clang::musttail]]
    return SearchStep();
  }

  {
    bool which = true;
    SearchState nextState = *this;

    nextState.hasReported = false;

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.pendingFocuses.currentState.state.SetCellUnsafe(cell, which);
    nextState.pendingFocuses.currentState.unknown.Erase(cell);
    nextState.pendingFocuses.currentState.unknownStable.Erase(cell);

    bool doRecurse = true;

    if (doRecurse) {
      bool consistent = nextState.stable.PropagateColumn(cell.first);
      doRecurse = consistent;
      if(consistent)
        nextState.TransferStableToCurrentColumn(cell.first);
    }

    if (doRecurse && pendingFocuses.isForcedInactive && stable.stateZOI.Get(focus)) {
      // See whether this choice keeps the focus stable in the next generation
      auto [focusNext, focusUnknown] =
          nextState.pendingFocuses.currentState.NextForCell(focus);
      doRecurse = focusUnknown || focusNext == nextState.stable.state.Get(focus);
    }

    if (doRecurse) {

      LifeUnknownState quicklook =
          nextState.pendingFocuses.currentState.UncertainStepFast(
              nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed =
        CheckConditionsOn(pendingFocuses.currentGen + 1, quicklook, nextState.stable, current,
                            quickactive, quickeveractive, activeTimer, streakTimer);
      doRecurse = conditionsPassed;
    }

    if (doRecurse)
      nextState.SearchStep();
  }
  {
    bool which = false;
    SearchState &nextState = *this; // Does not copy

    nextState.stable.state.SetCellUnsafe(cell, which);
    nextState.stable.unknownStable.Erase(cell);

    nextState.pendingFocuses.currentState.state.SetCellUnsafe(cell, which);
    nextState.pendingFocuses.currentState.unknown.Erase(cell);
    nextState.pendingFocuses.currentState.unknownStable.Erase(cell);

    bool doRecurse = true;

    if (doRecurse) {
      bool consistent = nextState.stable.PropagateColumn(cell.first);
      doRecurse = consistent;
      if(consistent)
        nextState.TransferStableToCurrentColumn(cell.first);
    }

    if (doRecurse && pendingFocuses.isForcedInactive && stable.stateZOI.Get(focus)) {
      // See whether this choice keeps the focus stable in the next generation
      auto [focusNext, focusUnknown] =
          nextState.pendingFocuses.currentState.NextForCell(focus);
      doRecurse = focusUnknown || focusNext == nextState.stable.state.Get(focus);
    }

    if (doRecurse) {
      LifeUnknownState quicklook =
          nextState.pendingFocuses.currentState.UncertainStepFast(
              nextState.stable);
      LifeState quickactive = quicklook.ActiveComparedTo(nextState.stable);
      LifeState quickeveractive = everActive | quickactive;
      bool conditionsPassed =
        CheckConditionsOn(pendingFocuses.currentGen + 1, quicklook, nextState.stable, current,
                            quickactive, quickeveractive, activeTimer, streakTimer);
      doRecurse = conditionsPassed;
    }

    if (doRecurse)
      [[clang::musttail]]
      return nextState.SearchStep();
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

bool SearchState::PassesFilter() const {
  if(currentGen > params->filterGen)
    return true;

  LifeUnknownState lookahead = current;
  for (unsigned lookaheadGen = currentGen; lookaheadGen < params->filterGen; lookaheadGen++) {
    lookahead = lookahead.UncertainStepMaintaining(stable);
  }

  bool allKnown = (params->filterMask & lookahead.unknown).IsEmpty();
  bool matches = ((lookahead.state ^ params->filterPattern) & params->filterMask).IsEmpty();
  return allKnown && matches;
}

void SearchState::ReportSolution() {
  if(params->pipeResults)
    ReportPipeSolution();
  else
    ReportFullSolution();
}

void SearchState::ReportFullSolution() {
  if (params->forbidEater2 && ContainsEater2(stable.state, everActive))
    return;

  if (params->filterGen != -1 && !PassesFilter())
    return;

  std::cout << "Winner:" << std::endl;
  std::cout << "x = 0, y = 0, rule = LifeBellman" << std::endl;
  LifeState starting = params->startingPattern;
  LifeState startingStableOff = params->startingStable & ~params->startingPattern;
  LifeState state = params->startingPattern | (stable.state & ~startingStableOff);
  LifeState marked = stable.unknownStable | (stable.state & ~startingStableOff);
  std::cout << LifeBellmanRLEFor(state, marked) << std::endl;

  if(params->stabiliseResults) {
    LifeState completed = stable.CompleteStable();

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
      std::cout << "Completed Plain:" << std::endl;
      std::cout << LifeState().RLE() << std::endl;
    }
  }
}

void SearchState::ReportPipeSolution() {
  if (params->forbidEater2 && ContainsEater2(stable.state, everActive))
    return;

  LifeState completed = stable.CompleteStable();

  if(completed.IsEmpty())
    return;

  LifeState starting = params->startingPattern;
  LifeState startingStableOff = params->startingStable & ~params->startingPattern;

  std::cout << "x = 0, y = 0, rule = B3/S23" << std::endl;
  std::cout << ((completed & ~startingStableOff) | starting).RLE() << "!" << std::endl << std::endl;
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

  if (params.maxCellActiveWindowGens != -1 && params.maxCellActiveWindowGens > maxCellActiveWindowGens) {
    std::cout << "max-cell-active-window is higher than allowed by the hardcoded value!" << std::endl;
    exit(1);
  }
  if (params.maxCellActiveStreakGens != -1 && params.maxCellActiveStreakGens > maxCellActiveStreakGens) {
    std::cout << "max-cell-active-streak is higher than allowed by the hardcoded value!" << std::endl;
    exit(1);
  }

  std::vector<LifeState> allSolutions;
  std::vector<uint64_t> seenRotors;

  SearchState search(params, allSolutions, seenRotors);
  search.Search();

  if (params.printSummary)
    PrintSummary(allSolutions);
}
