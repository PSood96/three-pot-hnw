#include "../include/base_game.hpp"

#include "config.hpp"
#include "hold_win.hpp"

#include <algorithm>
#include <cstdlib>

namespace threepot {

Board emptyBoard(SymId fill) {
  Board b{};
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      b[c][r] = Cell{fill, 0, 1.0, 0, false, SymId::BLANK};
  return b;
}

Board cloneBoard(const Board& b) {
  return b;
}

bool usedCoinType(const Board& b, SymId type) {
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (b[c][r].id == type) return true;
  return false;
}

bool reelHasCoin(const Board& b, int c) {
  for (int r = 0; r < ROWS; ++r)
    if (isCoin(b[c][r].id)) return true;
  return false;
}

std::vector<SymId> availableCoinTypes(const Board& b) {
  std::vector<SymId> out;
  for (int i = 0; i < 3; ++i) {
    SymId t = coinFromIndex(i);
    if (!usedCoinType(b, t)) out.push_back(t);
  }
  return out;
}

Cell makeCoin(SymId type, double val) {
  Cell cell;
  cell.id = type;
  cell.value = val;
  return cell;
}

Cell makeSym(SymId id) {
  Cell cell;
  cell.id = id;
  return cell;
}

static double pickCoinValue(Rng& rng) {
  return rng.pickWeighted(COIN_VALUE_W);
}

static SymId pickBaseSym(Rng& rng) {
  return rng.pickWeighted(SYM_WEIGHTS);
}

static void placeScatters(Board& b, Rng& rng, const std::vector<CoinPos>* forceCoins) {
  if (forceCoins && !forceCoins->empty()) {
    std::vector<int> reels = {0, 1, 2, 3, 4};
    rng.shuffle(reels);
    bool placedTypes[3] = {false, false, false};
    size_t ri = 0;
    for (size_t i = 0; i < forceCoins->size() && ri < reels.size(); ++i) {
      const CoinPos& fc = (*forceCoins)[i];
      SymId type = fc.cell.id;
      int ti = coinIndex(type);
      if (ti < 0 || placedTypes[ti]) continue;
      int c = reels[ri++];
      int r = rng.nextInt(ROWS);
      double val = fc.cell.value > 0 ? fc.cell.value : pickCoinValue(rng);
      b[c][r] = makeCoin(type, val);
      placedTypes[ti] = true;
    }
    return;
  }

  std::vector<SymId> typesLeft = {SymId::SC1, SymId::SC2, SymId::SC3};
  for (int c = 0; c < COLS; ++c) {
    if (typesLeft.empty()) break;
    if (!rng.chance(COIN_REEL_CHANCE)) continue;
    int ti = rng.nextInt(static_cast<int>(typesLeft.size()));
    SymId type = typesLeft[ti];
    typesLeft.erase(typesLeft.begin() + ti);
    int r = rng.nextInt(ROWS);
    b[c][r] = makeCoin(type, pickCoinValue(rng));
  }
}

Board buildBaseBoard(Rng& rng, const std::vector<CoinPos>* forceCoins) {
  Board b = emptyBoard(SymId::L5);
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      b[c][r] = makeSym(pickBaseSym(rng));
  placeScatters(b, rng, forceCoins);
  return b;
}

std::vector<CoinPos> listCoins(const Board& b) {
  std::vector<CoinPos> out;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (isCoin(b[c][r].id))
        out.push_back(CoinPos{c, r, b[c][r]});
  return out;
}

double cellCash(const Cell& cell) {
  double base = 0.0;
  if (cell.id == SymId::PRIZE) base = cell.value;
  else if (isJackpot(cell.id)) base = jpValue(cell.id);
  else if (isCoin(cell.id)) base = cell.value;
  else return 0.0;
  return round2(base * (cell.aoeMulti > 0 ? cell.aoeMulti : 1.0));
}

std::vector<LineWin> evaluateLines(const Board& b) {
  std::vector<LineWin> wins;
  for (int li = 0; li < 25; ++li) {
    const auto& pl = PAYLINES[li];
    SymId first = b[0][pl[0]].id;
    if (isCoin(first) || first == SymId::BLANK) continue;

    std::vector<std::pair<int, int>> positions;
    positions.push_back({0, pl[0]});
    SymId sym = first;
    for (int col = 1; col < COLS; ++col) {
      SymId next = b[col][pl[col]].id;
      if (isCoin(next) || next == SymId::BLANK) break;
      if (sym == SymId::WW) sym = next;
      if (next != sym && next != SymId::WW) break;
      positions.push_back({col, pl[col]});
    }

    int oak = static_cast<int>(positions.size());
    SymId paySym = sym;

    if (b[0][pl[0]].id == SymId::WW) {
      std::vector<std::pair<int, int>> wpos;
      wpos.push_back({0, pl[0]});
      int wi = 1;
      for (; wi < oak; ++wi) {
        if (b[wi][pl[wi]].id != SymId::WW) break;
        wpos.push_back({wi, pl[wi]});
      }
      int wOak = static_cast<int>(wpos.size());
      double wwPay = paytableWin(SymId::WW, wOak);
      double symPay = paytableWin(paySym, oak);
      if (wOak >= 3 && wwPay > 0 && (symPay <= 0 || wwPay > symPay)) {
        paySym = SymId::WW;
        oak = wOak;
        positions = wpos;
      }
    }

    double base = paytableWin(paySym, oak);
    if (oak >= 3 && base > 0) {
      LineWin w;
      w.payline = li;
      w.symbol = paySym;
      w.oak = oak;
      w.win = base;
      wins.push_back(w);
    }
  }
  return wins;
}

CoinPayout coinPayout(const std::vector<CoinPos>& coins) {
  CoinPayout cp;
  for (const auto& co : coins) cp.sum += co.cell.value;
  int n = static_cast<int>(coins.size());
  if (n >= 3) cp.multi = COMBO_MULTI_3;
  else if (n == 2) cp.multi = COMBO_MULTI_2;
  else cp.multi = 1.0;
  cp.total = round2(cp.sum * cp.multi);
  return cp;
}

int rollBonusPotCount(Rng& rng) {
  double r = rng.nextDouble();
  double p3 = BONUS_FREQ_3, p2 = BONUS_FREQ_2, p1 = BONUS_FREQ_1;
  if (r < p3) return 3;
  if (r < p3 + p2) return 2;
  if (r < p3 + p2 + p1) return 1;
  return 0;
}

std::vector<SymId> pickPotsForTier(Rng& rng, int n, const int potState[3]) {
  n = std::max(1, std::min(3, n));
  std::vector<SymId> charged, rest;
  for (int i = 0; i < 3; ++i) {
    SymId t = coinFromIndex(i);
    if (potState[i] >= POT_NEED) charged.push_back(t);
    else rest.push_back(t);
  }
  rng.shuffle(charged);
  rng.shuffle(rest);
  std::vector<SymId> picked;
  for (SymId t : charged) if (static_cast<int>(picked.size()) < n) picked.push_back(t);
  for (SymId t : rest) if (static_cast<int>(picked.size()) < n) picked.push_back(t);
  return picked;
}

void resolveActivePots(Rng& rng, const std::vector<CoinPos>& triggerCoins, int potState[3],
                       const std::vector<SymId>* forceActive, bool activeOut[3]) {
  activeOut[0] = activeOut[1] = activeOut[2] = false;
  if (forceActive && !forceActive->empty()) {
    for (SymId k : *forceActive) {
      int i = coinIndex(k);
      if (i >= 0) {
        activeOut[i] = true;
        potState[i] = POT_NEED;
      }
    }
    return;
  }

  bool present[3] = {false, false, false};
  for (const auto& co : triggerCoins) {
    int i = coinIndex(co.cell.id);
    if (i >= 0) present[i] = true;
  }
  for (int i = 0; i < 3; ++i) {
    if (potState[i] >= POT_NEED && present[i]) activeOut[i] = true;
  }
  if (activeOut[0] || activeOut[1] || activeOut[2]) return;

  std::vector<SymId> presentList;
  for (const auto& co : triggerCoins) {
    if (!isCoin(co.cell.id)) continue;
    if (std::find(presentList.begin(), presentList.end(), co.cell.id) == presentList.end())
      presentList.push_back(co.cell.id);
  }
  if (presentList.empty()) presentList.push_back(SymId::SC1);
  SymId pick = presentList[rng.nextInt(static_cast<int>(presentList.size()))];
  int pi = coinIndex(pick);
  activeOut[pi] = true;
  potState[pi] = POT_NEED;
}

std::vector<CoinPos> makeBuyTriggerCoins(Rng& rng, const std::vector<SymId>& activeList) {
  std::vector<SymId> types;
  for (SymId t : activeList) {
    if (std::find(types.begin(), types.end(), t) == types.end()) types.push_back(t);
  }
  for (int i = 0; i < 3; ++i) {
    if (static_cast<int>(types.size()) >= 3) break;
    SymId t = coinFromIndex(i);
    if (std::find(types.begin(), types.end(), t) == types.end()) types.push_back(t);
  }
  int n = std::min(3, std::max(1, static_cast<int>(types.size())));
  types.resize(n);
  std::vector<int> reels = {0, 1, 2, 3, 4};
  rng.shuffle(reels);
  double vals[3] = {1.0, 0.5, 1.0};
  std::vector<CoinPos> out;
  for (int i = 0; i < n; ++i) {
    CoinPos co;
    co.c = reels[i];
    co.r = i % ROWS;
    co.cell = makeCoin(types[i], vals[i]);
    out.push_back(co);
  }
  return out;
}

std::vector<SymId> pickBonusBuy1Pots(Rng& rng) {
  static const SymId opts[3] = {SymId::SC1, SymId::SC2, SymId::SC3};
  return {opts[rng.nextInt(3)]};
}

static std::vector<CoinPos> forceCoinList(const std::vector<SymId>& types) {
  std::vector<CoinPos> list;
  bool seen[3] = {false, false, false};
  for (SymId t : types) {
    int i = coinIndex(t);
    if (i < 0 || seen[i]) continue;
    seen[i] = true;
    CoinPos co;
    co.cell = makeCoin(t, 1.0);
    list.push_back(co);
  }
  return list;
}

RoundResult fastBaseRound(Rng& rng, int potState[3], Stats& st) {
  std::vector<SymId> forceActive;
  bool wantBonus = false;
  int tier = rollBonusPotCount(rng);
  if (tier > 0) {
    wantBonus = true;
    forceActive = pickPotsForTier(rng, tier, potState);
  }

  Board b;
  if (wantBonus) {
    auto force = forceCoinList(forceActive.empty() ? std::vector<SymId>{SymId::SC1} : forceActive);
    b = buildBaseBoard(rng, &force);
  } else {
    b = buildBaseBoard(rng, nullptr);
  }

  auto wins = evaluateLines(b);
  double lineWin = 0.0;
  for (const auto& w : wins) {
    lineWin += w.win;
    int si = paySymIndex(w.symbol);
    if (si >= 0 && w.oak >= 3 && w.oak <= 5) {
      HitWin& h = st.lineSymOak[si][w.oak - 3];
      h.hits++;
      h.won = round2(h.won + w.win * BET);
    }
  }
  lineWin = round2(lineWin * BET);
  st.lineWon = round2(st.lineWon + lineWin);

  auto coins = listCoins(b);
  auto cp = coinPayout(coins);
  double coinWin = round2(cp.total * BET);
  st.coinWon = round2(st.coinWon + coinWin);

  int nc = static_cast<int>(coins.size());
  if (nc > 3) nc = 3;
  st.coinByCount[nc].hits++;
  st.coinByCount[nc].won = round2(st.coinByCount[nc].won + coinWin);

  for (const auto& co : coins) {
    int i = coinIndex(co.cell.id);
    if (i >= 0) potState[i] = std::min(POT_NEED, potState[i] + 1);

    double share = round2(co.cell.value * cp.multi * BET);
    int vi = coinValueIndex(co.cell.value);
    if (vi >= 0) {
      st.coinByValue[vi].hits++;
      st.coinByValue[vi].won = round2(st.coinByValue[vi].won + share);
    }
    if (i >= 0) {
      st.coinByType[i].hits++;
      st.coinByType[i].won = round2(st.coinByType[i].won + share);
    }
  }

  double basePart = round2(lineWin + coinWin);
  double bonusWin = 0.0;
  if (wantBonus) {
    bool active[3] = {false, false, false};
    resolveActivePots(rng, coins, potState, &forceActive, active);
    st.fsTrig++;
    noteTierHit(active, st);
    bonusWin = fastHoldWin(rng, coins, active, potState, st);
    st.fsWon = round2(st.fsWon + bonusWin);
    st.featSum = round2(st.featSum + bonusWin);
    double fx = bonusWin / BET;
    if (fx > st.biggestFeatX) st.biggestFeatX = fx;
  }
  st.baseWon = round2(st.baseWon + basePart);
  return RoundResult{basePart, bonusWin, round2(basePart + bonusWin), static_cast<int>(coins.size())};
}

} // namespace threepot
