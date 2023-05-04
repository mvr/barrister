#include "LifeAPI.h"

struct SearchParams {
public:
  unsigned minFirstActiveGen;
  unsigned maxFirstActiveGen;
  // unsigned minActiveWindowGens;
  unsigned maxActiveWindowGens;
  unsigned minStableInterval;

  // unsigned maxStablePop;
  // std::pair<unsigned, unsigned> stableBounds;

  int maxActiveCells;
  // unsigned maxChanges;
  std::pair<int, int> activeBounds;

  int maxEverActiveCells;
  std::pair<int, int> everActiveBounds;

  int maxCellActiveWindowGens;

  LifeState startingPattern;
  LifeState activePattern;
  LifeState startingStable;
  LifeState searchArea;
  LifeState stator;
  bool hasStator;

  // TODO: of course we might want more than one of these
  int filterGen;
  LifeState filterMask;
  LifeState filterPattern;

  bool stabiliseResults;
  bool skipGlancing;
  bool forbidEater2;
  bool printSummary;
  bool pipeResults;

  bool debug;

  static SearchParams FromToml(toml::value &toml);
};

SearchParams SearchParams::FromToml(toml::value &toml) {
  SearchParams params;

  std::vector<int> firstRange = toml::find_or<std::vector<int>>(toml, "first-active-range", {0, 100});
  params.minFirstActiveGen = firstRange[0];
  params.maxFirstActiveGen = firstRange[1];

  std::vector<int> windowRange = toml::find_or<std::vector<int>>(toml, "active-window-range", {0, 100});
  // params.minActiveWindowGens = windowRange[0];
  params.maxActiveWindowGens = windowRange[1];

  params.minStableInterval = toml::find_or(toml, "min-stable-interval", 4);

  params.maxActiveCells = toml::find_or(toml, "max-active-cells", -1);

  std::vector<int> activeBounds = toml::find_or<std::vector<int>>(toml, "active-bounds", {-1, -1});
  params.activeBounds.first = activeBounds[0];
  params.activeBounds.second = activeBounds[1];

  params.maxEverActiveCells = toml::find_or(toml, "max-ever-active-cells", -1);

  std::vector<int> everActiveBounds = toml::find_or<std::vector<int>>(toml, "ever-active-bounds", {-1, -1});
  params.everActiveBounds.first = everActiveBounds[0];
  params.everActiveBounds.second = everActiveBounds[1];

  // params.maxChanges = toml::find_or(toml, "max-changed-cells", 100);

  params.maxCellActiveWindowGens = toml::find_or(toml, "max-cell-active-window", -1);

  params.stabiliseResults = toml::find_or(toml, "stabilise-results", true);
  params.skipGlancing = toml::find_or(toml, "skip-glancing", true);
  params.forbidEater2 = toml::find_or(toml, "forbid-eater2", false);
  params.printSummary = toml::find_or(toml, "print-summary", true);
  params.pipeResults = toml::find_or(toml, "pipe-results", false);
  if(params.pipeResults) {
    params.stabiliseResults = true;
    params.printSummary = false;
  }

  params.debug = toml::find_or(toml, "debug", false);

  std::string rle = toml::find<std::string>(toml, "pattern");
  LifeHistoryState pat = ParseLifeHistoryWHeader(rle);

  std::vector<int> patternCenter = toml::find_or<std::vector<int>>(toml, "pattern-center", {0, 0});

  pat.state.Move(-patternCenter[0], -patternCenter[1]);
  pat.marked.Move(-patternCenter[0], -patternCenter[1]);
  pat.history.Move(-patternCenter[0], -patternCenter[1]);
  pat.original.Move(-patternCenter[0], -patternCenter[1]);

  params.startingPattern = pat.state;
  params.activePattern = pat.state & ~pat.marked;
  params.startingStable = pat.marked;
  params.searchArea = pat.history;
  params.stator = pat.original;
  params.hasStator = !params.stator.IsEmpty();

  params.filterGen = toml::find_or(toml, "filter-gen", -1);
  if(params.filterGen != -1) {
    std::string rle = toml::find_or<std::string>(toml, "filter", "");
    LifeHistoryState pat = ParseLifeHistoryWHeader(rle);

    std::vector<int> patternCenter = toml::find_or<std::vector<int>>(toml, "filter-pos", {0, 0});
    pat.state.Move(patternCenter[0], patternCenter[1]);
    pat.marked.Move(patternCenter[0], patternCenter[1]);
    pat.history.Move(patternCenter[0], patternCenter[1]);
    pat.original.Move(patternCenter[0], patternCenter[1]);

    params.filterMask = pat.marked;
    params.filterPattern = pat.state;
  }
  return params;
}
