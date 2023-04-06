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
  LifeState stator;

  bool stabiliseResults;
  bool skipGlancing;
  bool forbidEater2;
  bool printSummary;

  SymmetryTransform symTransf;
  DomainChoice fundDomain;

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

  std::string symName = toml::find_or<std::string>(toml, "symmetry", "identity");
  params.symTransf = SymmetryTransform::Identity;
  if (symName.compare("D2|") == 0)
    params.symTransf = SymmetryTransform::ReflectAcrossY;
  else if (symName.compare("D2|even") == 0)
    params.symTransf = SymmetryTransform::ReflectAcrossYEven;
  else if (symName.compare("D2-") == 0)
    params.symTransf = SymmetryTransform::ReflectAcrossX;
  else if (symName.compare("D2-even") == 0)
    params.symTransf = SymmetryTransform::ReflectAcrossXEven;
  else if (symName.compare("C2botheven") == 0 || symName.compare("C2evenboth") == 0)
    params.symTransf = SymmetryTransform::Rotate180EvenBoth;
  else if (symName.compare("C2horizontaleven") == 0 || symName.compare("C2|even") == 0)
    params.symTransf = SymmetryTransform::Rotate180EvenHorizontal;
  else if (symName.compare("C2verticaleven") == 0 || symName.compare("C2-even") == 0)
    params.symTransf = SymmetryTransform::Rotate180EvenVertical;
  else if (symName.compare("C2") == 0 || symName.compare("C2oddboth") == 0 ||
                symName.compare("C2bothodd") == 0)
    params.symTransf = SymmetryTransform::Rotate180OddBoth;

  params.debug = toml::find_or(toml, "debug", false);

  std::string rle = toml::find<std::string>(toml, "pattern");
  LifeHistoryState pat = ParseLifeHistoryWHeader(rle);

  std::vector<int> patternCenter = toml::find_or<std::vector<int>>(toml, "pattern-center", {0, 0});

  pat.state.Move(-patternCenter[0], -patternCenter[1]);
  pat.marked.Move(-patternCenter[0], -patternCenter[1]);
  pat.history.Move(-patternCenter[0], -patternCenter[1]);
  pat.original.Move(-patternCenter[0], -patternCenter[1]);

  params.activePattern = pat.state & ~pat.marked;
  params.startingStable = pat.state & pat.marked;
  params.searchArea = pat.history;
  params.stator = pat.original;

  // we make a choice of fundamental domain based off whether
  // the prescribed search area
  std::array<SymmetryTransform, 4> rotationalSyms({Rotate180EvenBoth,
      Rotate180OddBoth, Rotate180EvenHorizontal, Rotate180EvenVertical});
  if(std::find(rotationalSyms.begin(), rotationalSyms.end(), params.symTransf)
          != rotationalSyms.end()){
    const LifeState yAxis = LifeState::SolidRect(0, 0, 1, 64);
    const LifeState xAxis = LifeState::SolidRect(0,0,N,1);
    if ((params.searchArea & yAxis).IsEmpty())
      params.fundDomain = DomainChoice::LEFTRIGHT;
    else if ((params.searchArea & xAxis).IsEmpty())
      params.fundDomain = DomainChoice::TOPBOTTOM;
    else {
      std::cout << "Unable to make domain choice for C2 symmetry";
      std::cout << " based off search area" << std::endl;
      exit(1);
    }
  } else
    params.fundDomain = DomainChoice::NONE;

  params.fundDomain = DomainChoice::TOPBOTTOM;

  return params;
}
