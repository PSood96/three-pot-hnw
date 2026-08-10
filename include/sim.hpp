#pragma once

#include "types.hpp"

#include <cstdint>
#include <string>

namespace threepot {

struct SimOptions {
  SimMode mode = SimMode::Main;
  long long spins = 100000;
  std::uint64_t seed = 1;
};

struct SimReport {
  SimMode mode = SimMode::Main;
  long long spins = 0;
  double stakePerSpin = 1.0;
  double staked = 0.0;
  double totalWon = 0.0;
  double baseWon = 0.0;
  double fsWon = 0.0;
  double rtp = 0.0;
  double meanX = 0.0;
  double hitRate = 0.0;
  double featureRate = 0.0;
  double biggestFeatX = 0.0;
  double bestX = 0.0;
  long long potUse[3] = {0, 0, 0};
  long long tierHits[4] = {0, 0, 0, 0};
  double seconds = 0.0;
  Stats detail{};
};

SimReport runSim(const SimOptions& opts);
std::string formatReport(const SimReport& r);

} // namespace threepot
