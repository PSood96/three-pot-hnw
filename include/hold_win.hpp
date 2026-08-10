#pragma once

#include "rng.hpp"
#include "types.hpp"

#include <vector>

namespace threepot {

double fastHoldWin(Rng& rng, const std::vector<CoinPos>& triggerCoins,
                   bool activePotsIn[3], int potState[3], Stats& st);

} // namespace threepot
