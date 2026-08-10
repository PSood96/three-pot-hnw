#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace threepot {

constexpr int ROWS = 3;
constexpr int COLS = 5;
constexpr int HW_CELLS = 15;
constexpr int POT_NEED = 6;
constexpr double BET = 1.0;

enum class SymId {
  WW, H1, H2, H3, H4, H5, L1, L2, L3, L4, L5,
  SC1, SC2, SC3,
  BLANK, PRIZE, MULTI, BOOST, GOLD_BOOST,
  MINI, MINOR, MAJOR, GRAND,
  COUNT
};

inline bool isCoin(SymId id) {
  return id == SymId::SC1 || id == SymId::SC2 || id == SymId::SC3;
}

inline bool isJackpot(SymId id) {
  return id == SymId::MINI || id == SymId::MINOR || id == SymId::MAJOR || id == SymId::GRAND;
}

inline const char* coinKey(SymId id) {
  switch (id) {
    case SymId::SC1: return "SC1";
    case SymId::SC2: return "SC2";
    case SymId::SC3: return "SC3";
    default: return "";
  }
}

inline int coinIndex(SymId id) {
  switch (id) {
    case SymId::SC1: return 0;
    case SymId::SC2: return 1;
    case SymId::SC3: return 2;
    default: return -1;
  }
}

inline SymId coinFromIndex(int i) {
  static const SymId k[3] = {SymId::SC1, SymId::SC2, SymId::SC3};
  return k[i];
}

struct Cell {
  SymId id = SymId::BLANK;
  double value = 0.0;
  double aoeMulti = 1.0;
  int multi = 0;
  bool locked = false;
  SymId fromScat = SymId::BLANK;
};

using Board = std::array<std::array<Cell, ROWS>, COLS>;

struct CoinPos {
  int c = 0;
  int r = 0;
  Cell cell;
};

struct LineWin {
  int payline = 0;
  SymId symbol = SymId::L5;
  int oak = 0;
  double win = 0.0;
};

struct CoinPayout {
  double sum = 0.0;
  double multi = 1.0;
  double total = 0.0;
};

struct RoundResult {
  double baseWin = 0.0;
  double bonusWin = 0.0;
  double total = 0.0;
  int coins = 0;
};

struct HitWin {
  long long hits = 0;
  double won = 0.0; // cash (same units as baseWon / fsWon)
};

constexpr int kPaySymCount = 11; // WW .. L5
constexpr int kOakCount = 3;     // 3, 4, 5
constexpr int kCoinValueCount = 5;
constexpr int kFillBandCount = 6; // 0–2, 3–5, 6–8, 9–11, 12–14, 15
constexpr int kJpCount = 4;       // MINI, MINOR, MAJOR, GRAND
constexpr int kCumBandCount = 7;  // <1, >=1, >=1.5, >=2, >=3, >=5, >=10
constexpr int kWinRangeCount = 20;

inline const char* paySymName(int i) {
  static const char* names[kPaySymCount] = {
    "WW", "H1", "H2", "H3", "H4", "H5", "L1", "L2", "L3", "L4", "L5"
  };
  if (i < 0 || i >= kPaySymCount) return "?";
  return names[i];
}

inline int paySymIndex(SymId id) {
  int i = static_cast<int>(id);
  if (i < 0 || i >= kPaySymCount) return -1;
  return i;
}

inline int coinValueIndex(double v) {
  static const double vals[kCoinValueCount] = {0.2, 0.5, 1.0, 1.5, 2.0};
  for (int i = 0; i < kCoinValueCount; ++i) {
    if (std::abs(vals[i] - v) < 1e-9) return i;
  }
  return -1;
}

inline double coinValueAt(int i) {
  static const double vals[kCoinValueCount] = {0.2, 0.5, 1.0, 1.5, 2.0};
  if (i < 0 || i >= kCoinValueCount) return 0.0;
  return vals[i];
}

/** Locked-cell fill band on a 15-cell board (strict coin count). */
inline int fillBand(int lockedCount) {
  if (lockedCount <= 2) return 0;  // 0–2
  if (lockedCount <= 5) return 1;  // 3–5
  if (lockedCount <= 8) return 2;  // 6–8
  if (lockedCount <= 11) return 3; // 9–11
  if (lockedCount <= 14) return 4; // 12–14
  return 5;                       // 15
}

inline const char* fillBandName(int b) {
  static const char* names[kFillBandCount] = {
    "0-2", "3-5", "6-8", "9-11", "12-14", "15"
  };
  if (b < 0 || b >= kFillBandCount) return "?";
  return names[b];
}

inline int jpIndex(SymId id) {
  switch (id) {
    case SymId::MINI: return 0;
    case SymId::MINOR: return 1;
    case SymId::MAJOR: return 2;
    case SymId::GRAND: return 3;
    default: return -1;
  }
}

inline const char* jpName(int i) {
  static const char* names[kJpCount] = {"MINI", "MINOR", "MAJOR", "GRAND"};
  if (i < 0 || i >= kJpCount) return "?";
  return names[i];
}

inline const char* cumBandName(int i) {
  static const char* names[kCumBandCount] = {
    "< 1x", ">= 1x", ">= 1.5x", ">= 2x", ">= 3x", ">= 5x", ">= 10x"
  };
  if (i < 0 || i >= kCumBandCount) return "?";
  return names[i];
}

inline const char* winRangeName(int i) {
  static const char* names[kWinRangeCount] = {
    "Win = (0)bet",
    "Win = (0-1]bet",
    "Win = (1-2]bet",
    "Win = (2-3]bet",
    "Win = (3-5]bet",
    "Win = (5-10]bet",
    "Win = (10-20]bet",
    "Win = (20-30]bet",
    "Win = (30-40]bet",
    "Win = (40-50]bet",
    "Win = (50-75]bet",
    "Win = (75-100]bet",
    "Win = (100-150]bet",
    "Win = (150-300]bet",
    "Win = (300-1000]bet",
    "Win = (1000-2000]bet",
    "Win = (2000-5000]bet",
    "Win = (5000-10000]bet",
    "Win = (10000-15000]bet",
    "Win = (15000-)bet"
  };
  if (i < 0 || i >= kWinRangeCount) return "?";
  return names[i];
}

/** Discrete win×bet range index. */
inline int winRangeIndex(double x) {
  if (x <= 0.0) return 0;
  if (x <= 1.0) return 1;
  if (x <= 2.0) return 2;
  if (x <= 3.0) return 3;
  if (x <= 5.0) return 4;
  if (x <= 10.0) return 5;
  if (x <= 20.0) return 6;
  if (x <= 30.0) return 7;
  if (x <= 40.0) return 8;
  if (x <= 50.0) return 9;
  if (x <= 75.0) return 10;
  if (x <= 100.0) return 11;
  if (x <= 150.0) return 12;
  if (x <= 300.0) return 13;
  if (x <= 1000.0) return 14;
  if (x <= 2000.0) return 15;
  if (x <= 5000.0) return 16;
  if (x <= 10000.0) return 17;
  if (x <= 15000.0) return 18;
  return 19;
}

struct Stats {
  long long spins = 0;
  double staked = 0.0;
  double baseWon = 0.0;
  double fsWon = 0.0;
  long long hits = 0;
  long long fsTrig = 0;
  double bestX = 0.0;
  double biggestFeatX = 0.0;
  double featSum = 0.0;
  long long potUse[3] = {0, 0, 0};
  long long tierHits[4] = {0, 0, 0, 0}; // index 1..3
  double sumX = 0.0;
  double sumX2 = 0.0;
  long long paidCount = 0;

  // Round win bands (× bet): cumulative thresholds + discrete ranges
  HitWin cumBand[kCumBandCount]{};
  HitWin winRange[kWinRangeCount]{};

  // Base: line pays by symbol × OAK (3/4/5)
  HitWin lineSymOak[kPaySymCount][kOakCount]{};
  double lineWon = 0.0;

  // Base: coins by count on spin (0–3), by face value, by type
  HitWin coinByCount[4]{};
  HitWin coinByValue[kCoinValueCount]{};
  HitWin coinByType[3]{};
  double coinWon = 0.0;

  // Bonus: fill bands by starting pot; features where that pot started
  long long hwFeatByPot[3] = {0, 0, 0};
  long long hwBoardsByPot[3] = {0, 0, 0};
  HitWin hwFillByPot[3][kFillBandCount]{};

  // Bonus: jackpots (cell JP + full-board GRAND)
  HitWin jp[kJpCount]{};
};

inline double round2(double n) {
  return std::round(n * 100.0) / 100.0;
}

inline void recordPaid(Stats& st, double x) {
  st.sumX += x;
  st.sumX2 += x * x;
  st.paidCount++;
  if (x > st.bestX) st.bestX = x;

  double cash = round2(x * BET);

  // Cumulative thresholds
  if (x < 1.0) {
    st.cumBand[0].hits++;
    st.cumBand[0].won = round2(st.cumBand[0].won + cash);
  }
  static const double cumThresh[kCumBandCount - 1] = {1.0, 1.5, 2.0, 3.0, 5.0, 10.0};
  for (int i = 0; i < kCumBandCount - 1; ++i) {
    if (x >= cumThresh[i]) {
      st.cumBand[i + 1].hits++;
      st.cumBand[i + 1].won = round2(st.cumBand[i + 1].won + cash);
    }
  }

  // Discrete win ranges
  int ri = winRangeIndex(x);
  st.winRange[ri].hits++;
  st.winRange[ri].won = round2(st.winRange[ri].won + cash);
}

inline void noteTierHit(const bool active[3], Stats& st) {
  int n = (active[0] ? 1 : 0) + (active[1] ? 1 : 0) + (active[2] ? 1 : 0);
  if (n >= 1 && n <= 3) st.tierHits[n]++;
}

enum class SimMode { Main, BB1, BB2, Pot1, Pot2, Pot3 };

inline const char* modeName(SimMode m) {
  switch (m) {
    case SimMode::Main: return "main";
    case SimMode::BB1: return "bb1";
    case SimMode::BB2: return "bb2";
    case SimMode::Pot1: return "pot1";
    case SimMode::Pot2: return "pot2";
    case SimMode::Pot3: return "pot3";
  }
  return "main";
}

} // namespace threepot
