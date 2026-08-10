#pragma once

#include "types.hpp"

#include <array>
#include <utility>
#include <vector>

namespace threepot {

struct FillTier {
  double maxFill;
  double land;
  double landLast; // -1 if unused
  double scat;
};

struct FsProfile {
  const char* id;
  const char* label;
  std::array<FillTier, 4> landByFill;
  std::vector<std::pair<std::string, double>> prizeW; // "0.5" or "MINI"
};

struct LadderStep {
  SymId id;
  double value; // only for PRIZE
};

// Constants
extern const double BUY_COST_X;
extern const double BUY_COST_ALL_X;
extern const double BONUS_FREQ_1;
extern const double BONUS_FREQ_2;
extern const double BONUS_FREQ_3;
extern const double COIN_REEL_CHANCE;
extern const double HW_BOOST_CHANCE;
extern const double HW_BOOST_CHANCE_AFTER5;
extern const int HW_BOOST_SOFT_CAP;
extern const int HW_BOOST_MAX_PER_SPIN;
extern const double HW_GOLD_BOOST_SHARE;
extern const double HW_MULTI_CHANCE;
extern const int HW_MULTI_MAX_PER_SPIN;
extern const double FULL_BOARD_GRAND;

extern const std::array<std::array<int, COLS>, 25> PAYLINES;
extern const std::vector<std::pair<SymId, double>> SYM_WEIGHTS;
extern const std::vector<std::pair<double, double>> COIN_VALUE_W; // value, weight
extern const double COMBO_MULTI_2;
extern const double COMBO_MULTI_3;

double paytableWin(SymId sym, int oak);
double jpValue(SymId id);

const FsProfile& fs1A();
const FsProfile& fs1B();
const FsProfile& fs1C();
const FsProfile& fs2();

const FsProfile& resolveFsWeights(const bool active[3], bool startedAllPots);

const FillTier& hwFillTier(int lockedCount, const FsProfile& fs);
double hwLandChance(int lockedCount, int livesLeft, const FsProfile& fs);
double hwScatterChance(int lockedCount, const FsProfile& fs);
double hwBoostChance(int boostsSeen);

const std::vector<std::pair<int, double>>& hwMultiW(); // multi value, weight
const std::vector<std::pair<std::string, double>>& hwMultiAoeW(); // reel/row/board
const std::vector<LadderStep>& hwValueLadder();

} // namespace threepot
