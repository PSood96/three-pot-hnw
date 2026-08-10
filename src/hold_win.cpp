#include "hold_win.hpp"

#include "base_game.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace threepot {

static int countLocked(const Board& b) {
  int n = 0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (b[c][r].locked) ++n;
  return n;
}

static bool boardFull(const Board& b) { return countLocked(b) >= COLS * ROWS; }

static double boardPrizeSum(const Board& b) {
  double sum = 0.0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      const Cell& cell = b[c][r];
      if (!cell.locked) continue;
      if (cell.id == SymId::MULTI || cell.id == SymId::BOOST || cell.id == SymId::GOLD_BOOST)
        continue;
      sum += cellCash(cell);
    }
  return round2(sum);
}

static SymId parsePrizeKey(const std::string& k, double& outVal) {
  if (k == "MINI") return SymId::MINI;
  if (k == "MINOR") return SymId::MINOR;
  if (k == "MAJOR") return SymId::MAJOR;
  if (k == "GRAND") return SymId::GRAND;
  outVal = std::stod(k);
  return SymId::PRIZE;
}

static Cell makeHwCell(Rng& rng, bool allowMulti, int lockedCount, bool allowBoost,
                       int livesLeft, int boostsSeen, const FsProfile& fsW,
                       bool allowGoldenBoost) {
  if (!rng.chance(hwLandChance(lockedCount, livesLeft, fsW))) {
    if (allowBoost && rng.chance(hwBoostChance(boostsSeen))) {
      Cell c;
      if (allowGoldenBoost && rng.chance(HW_GOLD_BOOST_SHARE))
        c.id = SymId::GOLD_BOOST;
      else
        c.id = SymId::BOOST;
      return c;
    }
    Cell c;
    c.id = SymId::BLANK;
    return c;
  }
  if (allowMulti && rng.chance(HW_MULTI_CHANCE)) {
    Cell c;
    c.id = SymId::MULTI;
    c.multi = rng.pickWeighted(hwMultiW());
    return c;
  }
  std::string key = rng.pickWeighted(fsW.prizeW);
  double val = 0.0;
  SymId id = parsePrizeKey(key, val);
  Cell c;
  c.id = id;
  c.locked = true;
  if (id == SymId::PRIZE) c.value = val;
  return c;
}

static void enforceHwSpecialCaps(Rng& rng, Board& next, const Board& lockedBoard,
                                 int maxBoost, int maxMulti) {
  struct Pos { int c, r; };
  std::vector<Pos> boosts, multis;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      if (lockedBoard[c][r].locked) continue;
      if (next[c][r].id == SymId::BOOST || next[c][r].id == SymId::GOLD_BOOST)
        boosts.push_back({c, r});
      else if (next[c][r].id == SymId::MULTI) multis.push_back({c, r});
    }
  auto keepRandom = [&](std::vector<Pos>& list, int max) {
    if (static_cast<int>(list.size()) <= max) return;
    rng.shuffle(list);
    for (size_t k = static_cast<size_t>(max); k < list.size(); ++k)
      next[list[k].c][list[k].r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
  };
  keepRandom(boosts, maxBoost);
  keepRandom(multis, maxMulti);
}

static void placeHwScatters(Rng& rng, Board& next, const Board& lockedBoard,
                            const bool activePots[3], const FsProfile& fsW) {
  int locked = countLocked(lockedBoard);
  double scatP = hwScatterChance(locked, fsW);
  std::vector<SymId> typesLeft;
  for (int i = 0; i < 3; ++i) {
    if (activePots[i]) continue;
    SymId t = coinFromIndex(i);
    if (!usedCoinType(lockedBoard, t) && !usedCoinType(next, t)) typesLeft.push_back(t);
  }
  if (typesLeft.empty()) return;

  for (int c = 0; c < COLS; ++c) {
    if (typesLeft.empty()) break;
    if (reelHasCoin(lockedBoard, c) || reelHasCoin(next, c)) continue;
    std::vector<int> freeRows;
    for (int r = 0; r < ROWS; ++r)
      if (!lockedBoard[c][r].locked) freeRows.push_back(r);
    if (freeRows.empty()) continue;
    if (!rng.chance(scatP)) continue;
    int ti = rng.nextInt(static_cast<int>(typesLeft.size()));
    SymId type = typesLeft[ti];
    typesLeft.erase(typesLeft.begin() + ti);
    int rr = freeRows[rng.nextInt(static_cast<int>(freeRows.size()))];
    // pick coin value like makeCoin(type)
    double val = rng.pickWeighted(COIN_VALUE_W);
    next[c][rr] = makeCoin(type, val);
  }
}

static std::string pickMultiAoeMode(Rng& rng) {
  return rng.pickWeighted(hwMultiAoeW());
}

static void applyMultiAoe(Rng& rng, Board& b, int atC, int atR, int multi) {
  if (multi <= 0) multi = 2;
  std::string mode = pickMultiAoeMode(rng);
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      if (mode == "reel" && c != atC) continue;
      if (mode == "row" && r != atR) continue;
      Cell& cell = b[c][r];
      if (!cell.locked) continue;
      if (cell.id == SymId::MULTI || cell.id == SymId::BOOST || cell.id == SymId::GOLD_BOOST)
        continue;
      double base = 0.0;
      if (cell.id == SymId::PRIZE) base = cell.value;
      else if (isJackpot(cell.id)) base = jpValue(cell.id);
      else if (isCoin(cell.id)) base = cell.value;
      if (base <= 0) continue;
      if (cell.aoeMulti <= 0) cell.aoeMulti = 1.0;
      cell.aoeMulti *= multi;
    }
}

static double lockedPrizeValue(const Cell& cell) {
  if (!cell.locked) return -1.0;
  if (cell.id == SymId::PRIZE) return cell.value;
  if (isJackpot(cell.id)) return jpValue(cell.id);
  return -1.0;
}

static void applyLadderStep(Cell& cell, const LadderStep& step) {
  if (step.id == SymId::PRIZE) {
    cell.id = SymId::PRIZE;
    cell.value = step.value;
  } else {
    cell.id = step.id;
    cell.value = 0;
  }
  cell.locked = true;
}

static LadderStep nextLadderAfter(double val) {
  for (const auto& step : hwValueLadder()) {
    double sv = step.id == SymId::PRIZE ? step.value : jpValue(step.id);
    if (sv > val) return step;
  }
  return LadderStep{SymId::BLANK, 0};
}

static void upgradeAllLowestToNextOnBoard(Board& b) {
  struct Entry { int c, r; Cell* cell; double v; };
  std::vector<Entry> entries;
  std::unordered_map<long long, bool> valueSet;
  auto keyOf = [](double v) { return static_cast<long long>(std::llround(v * 100.0)); };

  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r) {
      double v = lockedPrizeValue(b[c][r]);
      if (v < 0) continue;
      entries.push_back({c, r, &b[c][r], v});
      valueSet[keyOf(v)] = true;
    }
  if (entries.empty()) return;

  double minV = entries[0].v;
  for (const auto& e : entries) if (e.v < minV) minV = e.v;

  std::vector<double> distinct;
  for (const auto& kv : valueSet) distinct.push_back(kv.first / 100.0);
  std::sort(distinct.begin(), distinct.end());

  double nextV = -1.0;
  for (double d : distinct) {
    if (d > minV + 1e-9) { nextV = d; break; }
  }

  LadderStep targetStep{SymId::BLANK, 0};
  if (nextV >= 0) {
    for (int c = 0; c < COLS; ++c)
      for (int r = 0; r < ROWS; ++r) {
        double v2 = lockedPrizeValue(b[c][r]);
        if (std::abs(v2 - nextV) > 1e-9) continue;
        if (isJackpot(b[c][r].id)) {
          targetStep = LadderStep{b[c][r].id, 0};
        } else if (targetStep.id == SymId::BLANK) {
          targetStep = LadderStep{SymId::PRIZE, b[c][r].value};
        }
      }
  } else {
    targetStep = nextLadderAfter(minV);
  }
  if (targetStep.id == SymId::BLANK) return;

  for (auto& e : entries) {
    if (std::abs(e.v - minV) > 1e-9) continue;
    applyLadderStep(*e.cell, targetStep);
  }
}

static Board seedHwBoard(const std::vector<CoinPos>& triggerCoins) {
  Board b = emptyBoard(SymId::BLANK);
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      b[c][r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
  for (const auto& co : triggerCoins) {
    Cell cell;
    cell.id = SymId::PRIZE;
    cell.value = co.cell.value;
    cell.locked = true;
    cell.fromScat = co.cell.id;
    b[co.c][co.r] = cell;
  }
  return b;
}

double fastHoldWin(Rng& rng, const std::vector<CoinPos>& triggerCoins,
                   bool activePotsIn[3], int potState[3], Stats& st) {
  bool activePots[3] = {activePotsIn[0], activePotsIn[1], activePotsIn[2]};

  bool doubleBoard = activePots[1];
  int startLives = activePots[0] ? 4 : 3;
  bool useTrail = activePots[2];
  int boostsSeen = 0;
  bool startedAllPots = activePots[0] && activePots[1] && activePots[2];

  for (int i = 0; i < 3; ++i)
    if (activePots[i]) st.potUse[i]++;

  std::vector<Board> boards;
  boards.reserve(2);
  boards.push_back(seedHwBoard(triggerCoins));

  if (doubleBoard) {
    Board b2 = emptyBoard(SymId::BLANK);
    for (int c = 0; c < COLS; ++c)
      for (int r = 0; r < ROWS; ++r)
        b2[c][r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
    int seeds = 2 + rng.nextInt(2);
    for (int s = 0; s < seeds; ++s) {
      int cc = rng.nextInt(COLS), rr = rng.nextInt(ROWS);
      // makeHwCell with no multi/boost, lockedCount null → use 0, lives null → use 0, fs1A
      Cell cell = makeHwCell(rng, false, 0, false, 0, 0, fs1A(), false);
      if (cell.id != SymId::BLANK) {
        cell.locked = true;
        b2[cc][rr] = cell;
      } else {
        b2[cc][rr] = Cell{SymId::PRIZE, 0.5, 1.0, 0, true, SymId::BLANK};
      }
    }
    boards.push_back(b2);
  }

  std::vector<int> lives(boards.size(), startLives);

  auto lifeCap = [&]() { return activePots[0] ? 4 : 3; };

  auto spinOneBoard = [&](size_t bi) -> std::pair<int, bool> {
    Board& b = boards[bi];
    int newPrizes = 0;
    struct MultiPos { int c, r, multi; };
    std::vector<MultiPos> multiPos;
    bool landedBoost = false;
    bool landedGoldenBoost = false;
    struct MidCoin { int c, r; Cell cell; };
    std::vector<MidCoin> midCoins;

    int lockedNow = countLocked(b);
    const FsProfile& fsW = resolveFsWeights(activePots, startedAllPots);
    Board next = cloneBoard(b);
    for (int c = 0; c < COLS; ++c)
      for (int r = 0; r < ROWS; ++r) {
        if (b[c][r].locked) continue;
        next[c][r] = makeHwCell(rng, useTrail, lockedNow, useTrail, lives[bi], boostsSeen, fsW,
                                doubleBoard);
      }
    placeHwScatters(rng, next, b, activePots, fsW);
    enforceHwSpecialCaps(rng, next, b, HW_BOOST_MAX_PER_SPIN, HW_MULTI_MAX_PER_SPIN);

    for (int c = 0; c < COLS; ++c)
      for (int r = 0; r < ROWS; ++r) {
        if (b[c][r].locked) continue;
        Cell cell = next[c][r];
        if (cell.id == SymId::BLANK) {
          b[c][r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
          continue;
        }
        if (cell.id == SymId::BOOST || cell.id == SymId::GOLD_BOOST) {
          landedBoost = true;
          if (cell.id == SymId::GOLD_BOOST) landedGoldenBoost = true;
          b[c][r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
          continue;
        }
        if (cell.id == SymId::MULTI) {
          multiPos.push_back({c, r, cell.multi > 0 ? cell.multi : 2});
          b[c][r] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
          continue;
        }
        if (isCoin(cell.id)) {
          int ci = coinIndex(cell.id);
          potState[ci] = std::min(POT_NEED, potState[ci] + 1);
          midCoins.push_back({c, r, cell});
          cell.locked = true;
          b[c][r] = cell;
          ++newPrizes;
          continue;
        }
        cell.locked = true;
        b[c][r] = cell;
        ++newPrizes;
      }

    if (!multiPos.empty() && useTrail) {
      for (const auto& mp : multiPos)
        applyMultiAoe(rng, b, mp.c, mp.r, mp.multi);
    }
    if (landedBoost && useTrail) {
      ++boostsSeen;
      if (landedGoldenBoost) {
        // Revive any stopped (non-full) twin board: reset lives + upgrade lowest on both
        for (size_t li = 0; li < lives.size(); ++li)
          if (!boardFull(boards[li])) lives[li] = lifeCap();
        for (size_t obi = 0; obi < boards.size(); ++obi)
          upgradeAllLowestToNextOnBoard(boards[obi]);
      } else {
        lives[bi] = lifeCap();
        upgradeAllLowestToNextOnBoard(b);
      }
    }

    for (const auto& co : midCoins) {
      int ci = coinIndex(co.cell.id);
      potState[ci] = POT_NEED;
      if (!activePots[ci]) {
        activePots[ci] = true;
        st.potUse[ci]++;
        if (co.cell.id == SymId::SC1) {
          for (size_t li = 0; li < lives.size(); ++li)
            if (lives[li] > 0) lives[li] = std::min(4, lives[li] + 1);
        }
        if (co.cell.id == SymId::SC3) useTrail = true;
        if (co.cell.id == SymId::SC2 && !doubleBoard) {
          doubleBoard = true;
          Board nb = emptyBoard(SymId::BLANK);
          for (int c2 = 0; c2 < COLS; ++c2)
            for (int r2 = 0; r2 < ROWS; ++r2)
              nb[c2][r2] = Cell{SymId::BLANK, 0, 1.0, 0, false, SymId::BLANK};
          nb[1][1] = Cell{SymId::PRIZE, 1.0, 1.0, 0, true, SymId::BLANK};
          nb[3][1] = Cell{SymId::PRIZE, 0.5, 1.0, 0, true, SymId::BLANK};
          boards.push_back(nb);
          lives.push_back(activePots[0] ? 4 : 3);
        }
      }
    }
    return {newPrizes, landedBoost || !multiPos.empty()};
  };

  int guard = 0;
  while (guard++ < 400) {
    bool anyAlive = false;
    for (size_t bi = 0; bi < boards.size(); ++bi) {
      if (lives[bi] <= 0 || boardFull(boards[bi])) continue;
      anyAlive = true;
      auto spinRes = spinOneBoard(bi);
      if (spinRes.first > 0 || spinRes.second) lives[bi] = lifeCap();
      else lives[bi]--;
    }
    bool done = true;
    for (size_t bi2 = 0; bi2 < boards.size(); ++bi2) {
      if (lives[bi2] > 0 && !boardFull(boards[bi2])) done = false;
    }
    if (done || !anyAlive) break;
  }

  double total = 0.0;
  bool started[3] = {activePotsIn[0], activePotsIn[1], activePotsIn[2]};
  int nStart = (started[0] ? 1 : 0) + (started[1] ? 1 : 0) + (started[2] ? 1 : 0);
  if (nStart < 1) nStart = 1;
  for (int i = 0; i < 3; ++i)
    if (started[i]) st.hwFeatByPot[i]++;

  for (const auto& b : boards) {
    double bSum = boardPrizeSum(b);
    bool full = boardFull(b);
    double grandX = full ? FULL_BOARD_GRAND : 0.0;
    total = round2(total + bSum);
    total = round2(total + grandX);

    int locked = countLocked(b);
    int band = fillBand(locked);
    double boardCash = round2((bSum + grandX) * BET);
    double share = round2(boardCash / nStart);
    for (int i = 0; i < 3; ++i) {
      if (!started[i]) continue;
      st.hwBoardsByPot[i]++;
      st.hwFillByPot[i][band].hits++;
      st.hwFillByPot[i][band].won = round2(st.hwFillByPot[i][band].won + share);
    }

    for (int c = 0; c < COLS; ++c)
      for (int r = 0; r < ROWS; ++r) {
        const Cell& cell = b[c][r];
        if (!cell.locked) continue;
        int ji = jpIndex(cell.id);
        if (ji < 0) continue;
        double cash = round2(cellCash(cell) * BET);
        st.jp[ji].hits++;
        st.jp[ji].won = round2(st.jp[ji].won + cash);
      }
    if (full) {
      st.jp[3].hits++; // GRAND full-board award
      st.jp[3].won = round2(st.jp[3].won + FULL_BOARD_GRAND * BET);
    }
  }

  for (int i = 0; i < 3; ++i)
    if (activePots[i]) potState[i] = 0;
  return round2(total * BET);
}

} // namespace threepot
