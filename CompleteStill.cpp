#include "LifeAPI.h"
#include "Parsing.hpp"
#include "LifeStableState.hpp"

int main(int, char *argv[]) {
  LifeHistoryState input = ParseLifeHistoryWHeader(argv[1]);

  LifeStableState stable;
  stable.state = input.state;
  stable.unknownStable = input.history;

  std::cout << stable.state.RLE() << std::endl;
  std::cout << stable.unknownStable.RLE() << std::endl;
  LifeState result = stable.CompleteStable(3, true);
  std::cout << result.RLE() << std::endl;
}
