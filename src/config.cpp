#include "config.hpp"

#include <unordered_map>

namespace threepot {

const double BUY_COST_X = 80.0;
const double BUY_COST_ALL_X = 200.0;
const double BONUS_FREQ_2 = 1.0 / 1000.0;
const double BONUS_FREQ_3 = 1.0 / 100000.0;
const double BONUS_FREQ_1 = 1.0 / 220.0 - BONUS_FREQ_2 - BONUS_FREQ_3;
const double COIN_REEL_CHANCE = 0.016;
const double HW_BOOST_CHANCE = 0.032;
const double HW_BOOST_CHANCE_AFTER5 = 0.010;
const int HW_BOOST_SOFT_CAP = 5;
const int HW_BOOST_MAX_PER_SPIN = 1;
const double HW_GOLD_BOOST_SHARE = 0.20;
const double HW_MULTI_CHANCE = 0.14;
const int HW_MULTI_MAX_PER_SPIN = 2;
const double FULL_BOARD_GRAND = 2000.0;
const double COMBO_MULTI_2 = 1.5;
const double COMBO_MULTI_3 = 2.0;

const std::array<std::array<int, COLS>, 25> PAYLINES = {{
  {{1,1,1,1,1}}, {{0,0,0,0,0}}, {{2,2,2,2,2}},
  {{0,1,2,1,0}}, {{2,1,0,1,2}},
  {{0,0,1,0,0}}, {{2,2,1,2,2}},
  {{1,0,0,0,1}}, {{1,2,2,2,1}},
  {{0,1,1,1,0}}, {{2,1,1,1,2}},
  {{1,0,1,0,1}}, {{1,2,1,2,1}},
  {{0,1,0,1,0}}, {{2,1,2,1,2}},
  {{1,1,0,1,1}}, {{1,1,2,1,1}},
  {{0,0,0,1,2}}, {{2,2,2,1,0}},
  {{0,1,2,2,2}}, {{2,1,0,0,0}},
  {{0,0,1,2,2}}, {{2,2,1,0,0}},
  {{1,0,1,2,1}}, {{1,2,1,0,1}}
}};

const std::vector<std::pair<SymId, double>> SYM_WEIGHTS = {
  {SymId::WW, 5}, {SymId::H1, 5}, {SymId::H2, 7}, {SymId::H3, 9},
  {SymId::H4, 11}, {SymId::H5, 11},
  {SymId::L1, 20}, {SymId::L2, 20}, {SymId::L3, 22}, {SymId::L4, 22}, {SymId::L5, 22}
};

const std::vector<std::pair<double, double>> COIN_VALUE_W = {
  {0.2, 15}, {0.5, 25}, {1.0, 20}, {1.5, 15}, {2.0, 5}
};

static const std::unordered_map<SymId, std::array<double, 6>>& paytable() {
  static const std::unordered_map<SymId, std::array<double, 6>> t = {
    // index = oak (3,4,5 used)
    {SymId::WW, {{0,0,0, 2, 5, 10}}},
    {SymId::H1, {{0,0,0, 2, 5, 10}}},
    {SymId::H2, {{0,0,0, 1, 4, 8}}},
    {SymId::H3, {{0,0,0, 1, 4, 8}}},
    {SymId::H4, {{0,0,0, 1, 3, 5}}},
    {SymId::H5, {{0,0,0, 1, 3, 5}}},
    {SymId::L1, {{0,0,0, 0.5, 1, 2}}},
    {SymId::L2, {{0,0,0, 0.5, 1, 2}}},
    {SymId::L3, {{0,0,0, 0.5, 1, 2}}},
    {SymId::L4, {{0,0,0, 0.5, 1, 2}}},
    {SymId::L5, {{0,0,0, 0.5, 1, 2}}},
  };
  return t;
}

double paytableWin(SymId sym, int oak) {
  if (oak < 3 || oak > 5) return 0.0;
  auto it = paytable().find(sym);
  if (it == paytable().end()) return 0.0;
  return it->second[oak];
}

double jpValue(SymId id) {
  switch (id) {
    case SymId::MINI: return 10.0;
    case SymId::MINOR: return 25.0;
    case SymId::MAJOR: return 100.0;
    case SymId::GRAND: return 2000.0;
    default: return 0.0;
  }
}

static FsProfile makeFs(const char* id, const char* label,
                        std::array<FillTier, 4> land,
                        std::vector<std::pair<std::string, double>> prize) {
  FsProfile p;
  p.id = id;
  p.label = label;
  p.landByFill = land;
  p.prizeW = std::move(prize);
  return p;
}

const FsProfile& fs1A() {
  static const FsProfile p = makeFs(
    "fs1A", "Pot1 Extra Life",
    {{
      {1.0/3.0, 0.07, 0.25, 0.13},
      {1.0/2.0, 0.07, 0.20, 0.065},
      {3.0/4.0, 0.07, -1.0, 0.03},
      {1.01, 0.0025, -1.0, 0.01}
    }},
    {{"0.5",30},{"1",30},{"1.5",20},{"2",24},{"2.5",15},{"5",15},{"10",2},
     {"MINI",1},{"MINOR",0.02},{"MAJOR",0.01}}
  );
  return p;
}

const FsProfile& fs1B() {
  static const FsProfile p = makeFs(
    "fs1B", "Pot2 Double Board",
    {{
      {1.0/3.0, 0.07, 0.25, 0.081},
      {1.0/2.0, 0.05, 0.20, 0.045},
      {3.0/4.0, 0.10, -1.0, 0.02},
      {1.01, 0.001, -1.0, 0.001}
    }},
    {{"0.5",30},{"1",50},{"1.5",20},{"2",14},{"2.5",10},{"5",7},{"10",1},
     {"MINI",1},{"MINOR",0.02},{"MAJOR",0.01}}
  );
  return p;
}

const FsProfile& fs1C() {
  static const FsProfile p = makeFs(
    "fs1C", "Pot3 Vault 2x + BOOST",
    {{
      {1.0/3.0, 0.05, 0.25, 0.10},
      {1.0/2.0, 0.05, 0.15, 0.05},
      {3.0/4.0, 0.10, -1.0, 0.02},
      {1.01, 0.005, -1.0, 0.0018}
    }},
    {{"0.5",33},{"1",50},{"1.5",10},{"2",14},{"2.5",10},{"5",7},{"10",1},
     {"MINI",1},{"MINOR",0.05},{"MAJOR",0.01}}
  );
  return p;
}

const FsProfile& fs2() {
  static const FsProfile p = makeFs(
    "fs2", "Bonus Buy 2 · All pots",
    {{
      {1.0/3.0, 0.15, 0.35, 0.0},
      {1.0/2.0, 0.15, 0.25, 0.0},
      {3.0/4.0, 0.20, -1.0, 0.0},
      {1.01, 0.03, -1.0, 0.0}
    }},
    {{"0.5",0},{"1",14},{"1.5",20},{"2",30},{"2.5",30},{"5",30},{"10",5},
     {"MINI",5},{"MINOR",0.1},{"MAJOR",0.02}}
  );
  return p;
}

const FsProfile& resolveFsWeights(const bool active[3], bool startedAllPots) {
  if (startedAllPots) return fs2();
  if (active[2]) return fs1C();
  if (active[1]) return fs1B();
  return fs1A();
}

const FillTier& hwFillTier(int lockedCount, const FsProfile& fs) {
  double fill = (lockedCount) / static_cast<double>(HW_CELLS);
  for (const auto& t : fs.landByFill) {
    if (fill < t.maxFill) return t;
  }
  return fs.landByFill.back();
}

double hwLandChance(int lockedCount, int livesLeft, const FsProfile& fs) {
  const FillTier& tier = hwFillTier(lockedCount, fs);
  if (livesLeft == 1 && tier.landLast >= 0.0) return tier.landLast;
  return tier.land;
}

double hwScatterChance(int lockedCount, const FsProfile& fs) {
  return hwFillTier(lockedCount, fs).scat;
}

double hwBoostChance(int boostsSeen) {
  return boostsSeen >= HW_BOOST_SOFT_CAP ? HW_BOOST_CHANCE_AFTER5 : HW_BOOST_CHANCE;
}

const std::vector<std::pair<int, double>>& hwMultiW() {
  static const std::vector<std::pair<int, double>> w = {{2, 100}};
  return w;
}

const std::vector<std::pair<std::string, double>>& hwMultiAoeW() {
  static const std::vector<std::pair<std::string, double>> w = {
    {"reel", 40}, {"row", 40}, {"board", 20}
  };
  return w;
}

const std::vector<LadderStep>& hwValueLadder() {
  static const std::vector<LadderStep> ladder = {
    {SymId::PRIZE, 0.5}, {SymId::PRIZE, 1.0}, {SymId::PRIZE, 1.5}, {SymId::PRIZE, 2.0},
    {SymId::PRIZE, 2.5}, {SymId::PRIZE, 5.0}, {SymId::PRIZE, 10.0},
    {SymId::MINI, 0}, {SymId::MINOR, 0}, {SymId::MAJOR, 0}
  };
  return ladder;
}

} // namespace threepot
