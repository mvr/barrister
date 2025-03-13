#pragma once

#include "toml/toml.hpp"

#include "LifeAPI.h"
#include "LifeHistoryState.hpp"
#include "LifeUnknownState.hpp"
#include "LifeStableState.hpp"
#include "Parsing.hpp"

enum class FilterType {
  EXACT,
  EVER
};

struct Filter {
  LifeState mask;
  LifeState state;
  unsigned gen;
  FilterType type;
};

struct Forbidden {
  LifeState mask;
  LifeState state;
};

struct SearchParams {
public:
  unsigned minFirstActiveGen;
  unsigned maxFirstActiveGen;
  unsigned minActiveWindowGens;
  unsigned maxActiveWindowGens;
  unsigned minStableInterval;

  // unsigned maxStablePop;
  // std::pair<unsigned, unsigned> stableBounds;

  int maxActiveCells;
  int maxComponentActiveCells;
  std::pair<int, int> activeBounds;

  int maxEverActiveCells;
  std::pair<int, int> everActiveBounds;
  int maxComponentEverActiveCells;
  std::pair<int, int> componentEverActiveBounds;

  int changesGrace;
  int maxChanges;
  std::pair<int, int> changesBounds;
  int maxComponentChanges;
  std::pair<int, int> componentChangesBounds;

  bool usesChanges;

  int maxCellActiveWindowGens;
  int maxCellActiveStreakGens;

  int maxCellStationaryDistance;
  int maxCellStationaryStreakGens;

  LifeUnknownState startingState;
  LifeStableState stable;
  LifeState stator;
  LifeState exempt;
  bool hasStator;

  bool hasFilter;
  std::vector<Filter> filters;

  bool hasForbidden;
  std::vector<Forbidden> forbiddens;

  bool metasearch;
  unsigned metasearchRounds;
  unsigned minMetaFirstActiveGen;
  unsigned maxMetaFirstActiveGen;

  bool stabiliseResults;
  unsigned stabiliseResultsTimeout;
  bool minimiseResults;
  bool trimResults;
  unsigned minTrimHashes;
  bool reportOscillators;
  unsigned reportOscillatorsMinPeriod;
  std::string knownrotorsFile;
  bool continueAfterSuccess;
  bool printSummary;
  bool pipeResults;
  std::string outputFile;

  bool debug;
  bool hasOracle;
  LifeStableState oracle;

  static SearchParams FromToml(toml::value &toml);
};

SearchParams SearchParams::FromToml(toml::value &toml) {
  SearchParams params;

  std::vector<int> firstRange = toml::find_or<std::vector<int>>(toml, "first-active-range", {0, 100});
  params.minFirstActiveGen = firstRange[0];
  params.maxFirstActiveGen = firstRange[1];

  std::vector<int> windowRange = toml::find_or<std::vector<int>>(toml, "active-window-range", {0, 100});
  params.minActiveWindowGens = windowRange[0];
  params.maxActiveWindowGens = windowRange[1];

  params.minStableInterval = toml::find_or(toml, "min-stable-interval", 4);

  params.maxActiveCells = toml::find_or(toml, "max-active-cells", -1);
  params.maxComponentActiveCells = toml::find_or(toml, "max-component-active-cells", -1);
  std::vector<int> activeBounds = toml::find_or<std::vector<int>>(toml, "active-bounds", {-1, -1});
  params.activeBounds.first = activeBounds[0];
  params.activeBounds.second = activeBounds[1];

  params.maxEverActiveCells = toml::find_or(toml, "max-ever-active-cells", -1);
  std::vector<int> everActiveBounds = toml::find_or<std::vector<int>>(toml, "ever-active-bounds", {-1, -1});
  params.everActiveBounds.first = everActiveBounds[0];
  params.everActiveBounds.second = everActiveBounds[1];
  params.maxComponentEverActiveCells = toml::find_or(toml, "max-component-ever-active", -1);
  std::vector<int> componentEverActiveBounds = toml::find_or<std::vector<int>>(toml, "component-ever-active-bounds", {-1, -1});
  params.componentEverActiveBounds.first = componentEverActiveBounds[0];
  params.componentEverActiveBounds.second = componentEverActiveBounds[1];

  params.maxCellActiveWindowGens = toml::find_or(toml, "max-cell-active-window", -1);
  params.maxCellActiveStreakGens = toml::find_or(toml, "max-cell-active-streak", -1);

  params.changesGrace = toml::find_or(toml, "changes-grace", 0);
  params.maxChanges = toml::find_or(toml, "max-changes", -1);
  std::vector<int> changesBounds = toml::find_or<std::vector<int>>(toml, "changes-bounds", {-1, -1});
  params.changesBounds.first = changesBounds[0];
  params.changesBounds.second = changesBounds[1];
  params.maxComponentChanges = toml::find_or(toml, "max-component-changes", -1);
  std::vector<int> componentChangesBounds = toml::find_or<std::vector<int>>(toml, "component-changes-bounds", {-1, -1});
  params.componentChangesBounds.first = componentChangesBounds[0];
  params.componentChangesBounds.second = componentChangesBounds[1];

  params.maxCellStationaryDistance = toml::find_or(toml, "max-cell-stationary-distance", -1);

  params.usesChanges = params.maxChanges != -1 ||
                       params.changesBounds.first != -1 ||
                       params.maxComponentChanges != -1 ||
                       params.componentChangesBounds.first != -1 ||
                       params.maxCellStationaryDistance != -1 ||
                       params.maxCellStationaryStreakGens != -1;

  params.stabiliseResults = toml::find_or(toml, "stabilise-results", true);
  params.stabiliseResultsTimeout = toml::find_or(toml, "stabilise-results-timeout", 3);
  params.minimiseResults = toml::find_or(toml, "minimise-results", false);

  params.trimResults = toml::find_or(toml, "trim-results", true);
  params.minTrimHashes = toml::find_or(toml, "min-trim-hashes", 5);

  params.reportOscillators = toml::find_or(toml, "report-oscillators", false);
  params.reportOscillatorsMinPeriod = toml::find_or(toml, "report-oscillators-min-period", 5);
  params.knownrotorsFile = toml::find_or(toml, "knownrotors-file", "knownrotors");
  params.continueAfterSuccess = toml::find_or(toml, "continue-after-success", false);
  params.printSummary = toml::find_or(toml, "print-summary", true);

  params.pipeResults = toml::find_or(toml, "pipe-results", false);
  if(params.pipeResults) {
    params.stabiliseResults = true;
    params.stabiliseResultsTimeout = 1;
    params.minimiseResults = false;
    params.printSummary = false;
  }
  params.outputFile = toml::find_or(toml, "output-file", "");

  std::string rle = toml::find<std::string>(toml, "pattern");
  LifeHistoryState pat = LifeHistoryState::ParseWHeader(rle);

  std::vector<int> patternCenterVec = toml::find_or<std::vector<int>>(toml, "pattern-center", {-1, -1});
  std::pair<int, int> patternShift;
  if (patternCenterVec[0] != -1) {
    patternShift = {-patternCenterVec[0], -patternCenterVec[1]};
  } else {
    auto wh = (pat.state | pat.history).WidthHeight();
    patternShift = {-wh.first, -wh.second};
  }
  pat.Move(patternShift);

  params.stable.state = pat.marked | pat.original;
  params.stable.unknown = pat.history;

  if (toml::find_or(toml, "exempt-existing", true)) {
    params.exempt = (pat.marked | pat.original).ZOI() & ~pat.original;
  }

  params.startingState.state = pat.state;
  params.startingState.unknown = pat.history;
  params.startingState.unknownStable = pat.history;

  // This needs to be done in this order first, because the counts/options start at all 0
  params.stable.SynchroniseStateKnown();
  params.stable.Propagate();
  params.startingState.TransferStable(params.stable);

  params.stator = pat.original;
  params.hasStator = !params.stator.IsEmpty();

  if(toml.contains("filter")) {
    params.hasFilter = true;

    auto filters = toml::find<std::vector<toml::value>>(toml, "filter");
    for(auto &f : filters) {
      std::string rle = toml::find_or<std::string>(f, "filter", "");
      std::vector<int> filterCenterVec = toml::find_or<std::vector<int>>(f, "filter-pos", {0, 0});
      LifeHistoryState pat = LifeHistoryState::ParseWHeader(rle);
      unsigned filterGen = toml::find_or(f, "filter-gen", -1);

      FilterType filterType;
      std::string filterTypeStr = toml::find_or<std::string>(f, "filter-type", "EVER");
      if (filterTypeStr == "EXACT") {
        filterType = FilterType::EXACT;
      }
      if (filterTypeStr == "EVER") {
        filterType = FilterType::EVER;
      }

      pat.Move(filterCenterVec[0], filterCenterVec[1]);

      params.filters.push_back({pat.marked, pat.state, filterGen, filterType});
    }
  } else {
    params.hasFilter = false;
  }

  if(toml.contains("forbidden")) {
    params.hasForbidden = true;

    auto forbiddens = toml::find<std::vector<toml::value>>(toml, "forbidden");
    for(auto &f : forbiddens) {
      std::string rle = toml::find_or<std::string>(f, "forbidden", "");
      std::vector<int> forbiddenCenterVec =
        toml::find_or<std::vector<int>>(f, "forbidden-pos", {0, 0});
      LifeHistoryState pat = LifeHistoryState::ParseWHeader(rle);

      pat.Move(forbiddenCenterVec[0], forbiddenCenterVec[1]);

      params.forbiddens.push_back({pat.marked, pat.state});
    }
  } else {
    params.hasForbidden = false;
  }

  params.metasearch = toml::find_or(toml, "metasearch", false);
  params.metasearchRounds = toml::find_or(toml, "metasearch-rounds", 5);
  std::vector<unsigned> metaFirstRange = toml::find_or<std::vector<unsigned>>(toml, "meta-first-active-range", {params.minFirstActiveGen, params.maxFirstActiveGen});
  params.minMetaFirstActiveGen = metaFirstRange[0];
  params.maxMetaFirstActiveGen = metaFirstRange[1];

  params.debug = toml::find_or(toml, "debug", false);
  
  if (toml.contains("oracle")) {
    std::string oraclerle = toml::find<std::string>(toml, "oracle");
    LifeHistoryState oracle = LifeHistoryState::ParseWHeader(oraclerle);

    std::vector<int> oracleCenterVec = toml::find_or<std::vector<int>>(toml, "oracle-center", {0, 0});
    std::pair<int, int> oracleCenter = {-oracleCenterVec[0], -oracleCenterVec[1]};
    oracle.Move(oracleCenter);

    params.hasOracle = true;
    params.oracle = LifeStableState();
    params.oracle.state = (oracle.state & oracle.marked) | oracle.original;
    params.oracle.StabiliseOptions();
  } else {
    params.hasOracle = false;
  }

  return params;
}
