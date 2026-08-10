#pragma once

#include "rng.hpp"
#include "types.hpp"

#include <vector>

namespace threepot {

Board emptyBoard(SymId fill = SymId::L5);
Board cloneBoard(const Board& b);

bool usedCoinType(const Board& b, SymId type);
bool reelHasCoin(const Board& b, int c);
std::vector<SymId> availableCoinTypes(const Board& b);

Cell makeCoin(SymId type, double val);
Cell makeSym(SymId id);

Board buildBaseBoard(Rng& rng, const std::vector<CoinPos>* forceCoins = nullptr);

std::vector<CoinPos> listCoins(const Board& b);
std::vector<LineWin> evaluateLines(const Board& b);
CoinPayout coinPayout(const std::vector<CoinPos>& coins);

double cellCash(const Cell& cell);

int rollBonusPotCount(Rng& rng);
std::vector<SymId> pickPotsForTier(Rng& rng, int n, const int potState[3]);
void resolveActivePots(Rng& rng, const std::vector<CoinPos>& triggerCoins, int potState[3],
                       const std::vector<SymId>* forceActive, bool activeOut[3]);

std::vector<CoinPos> makeBuyTriggerCoins(Rng& rng, const std::vector<SymId>& activeList);
std::vector<SymId> pickBonusBuy1Pots(Rng& rng);

RoundResult fastBaseRound(Rng& rng, int potState[3], Stats& st);

} // namespace threepot
