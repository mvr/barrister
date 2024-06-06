#include <set>

#include "LifeAPI.h"
#include "Parsing.hpp"
#include "LifeStableState.hpp"
#include "RotorDescription.hpp"

std::pair<LifeState, unsigned> DeterminePeriod(const LifeState &state) {
  LifeState current = state;

  std::stack<std::pair<uint64_t, int>> minhashes;

  for (unsigned i = 1; i < 1000; i++) {
    uint64_t newhash = current.GetHash();

    while(true) {
      if(minhashes.empty())
        break;
      if(minhashes.top().first < newhash)
        break;

      if(minhashes.top().first == newhash) {
        unsigned p = i - minhashes.top().second;
        return {current, p};
      }

      if(minhashes.top().first > newhash)
        minhashes.pop();
    }

    minhashes.push({newhash, i});

    current.Step();
  }
  return {current, 0};
}

int main(int argc, char *argv[]) {
  std::set<std::string> seenRotors;

  std::string rotorfile = "knownrotors";
  if (argc > 1)
    rotorfile = argv[1];
  for (auto r : ReadRotors(rotorfile))
    seenRotors.insert(r);

  std::string rle;
  for (std::string line; std::getline(std::cin, line);) {
    if(line[0] != 'x') {
      rle.append(line);
      rle.append("\n");
    }
  }

  LifeState input = LifeState::Parse(rle.c_str());
  auto [osc, period] = DeterminePeriod(input);

  if (period == 0)
    std::cout << "Not Oscillating?" << std::endl;

  std::cout << "Oscillating! Period: " << period << std::endl;

  LifeUnknownState unknownState {osc, LifeState(), LifeState()};
  LifeStableState stable;
  stable.SetOn(osc);
  stable.SetOff(~osc);

  for(auto &r : GetSeparatedRotorDesc(unknownState, stable, period)) {
    auto rotorDesc = r.ToString();
    if (seenRotors.contains(rotorDesc))
      std::cout << "Known Rotor: " << rotorDesc << std::endl;
    else {
      seenRotors.insert(rotorDesc);
      std::cout << "New Rotor: " << rotorDesc << std::endl;
    }
  }

}
