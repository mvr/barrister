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

  unsigned maxActiveCells;
  unsigned maxActiveOnCells;
  // unsigned maxChanges;
  std::pair<unsigned, unsigned> activeBounds;

  unsigned maxEverActiveCells;
  std::pair<unsigned, unsigned> everActiveBounds;

  LifeState activePattern;
  LifeState startingStable;
  LifeState searchArea;

  bool stabiliseResults;
  bool skipGlancing;
  bool forbidEater2;
  bool printSummary;

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

  params.maxActiveCells = toml::find_or(toml, "max-active-cells", 20);

  std::vector<int> activeBounds = toml::find_or<std::vector<int>>(toml, "active-bounds", {100, 100});
  params.activeBounds.first = activeBounds[0];
  params.activeBounds.second = activeBounds[1];

  params.maxEverActiveCells = toml::find_or(toml, "max-ever-active-cells", 100);

  std::vector<int> everActiveBounds = toml::find_or<std::vector<int>>(toml, "ever-active-bounds", {100, 100});
  params.everActiveBounds.first = everActiveBounds[0];
  params.everActiveBounds.second = everActiveBounds[1];

  // params.maxChanges = toml::find_or(toml, "max-changed-cells", 100);

  params.stabiliseResults = toml::find_or(toml, "stabilise-results", true);
  params.skipGlancing = toml::find_or(toml, "skip-glancing", true);
  params.forbidEater2 = toml::find_or(toml, "forbid-eater2", false);
  params.printSummary = toml::find_or(toml, "print-summary", true);

  params.debug = toml::find_or(toml, "debug", false);

  LifeState stateon;
  LifeState statemarked;

  std::string rle = toml::find<std::string>(toml, "pattern");
  ParseTristateWHeader(rle, stateon, statemarked);

  std::vector<int> patternCenter = toml::find_or<std::vector<int>>(toml, "pattern-center", {0, 0});
  stateon.Move(-patternCenter[0], -patternCenter[1]);
  statemarked.Move(-patternCenter[0], -patternCenter[1]);

  params.activePattern = stateon & ~statemarked;
  params.startingStable = stateon & statemarked;
  params.searchArea = ~stateon & statemarked;

  return params;
}
