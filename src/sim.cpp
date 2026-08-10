#include "sim.hpp"

#include "base_game.hpp"
#include "config.hpp"
#include "hold_win.hpp"
#include "rng.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace threepot {

static double stakeForMode(SimMode mode) {
  switch (mode) {
    case SimMode::Main: return BET;
    case SimMode::BB2: return BET * BUY_COST_ALL_X;
    default: return BET * BUY_COST_X; // bb1, pot1-3
  }
}

static std::vector<SymId> potsForBuyMode(Rng& rng, SimMode mode) {
  switch (mode) {
    case SimMode::BB1: return pickBonusBuy1Pots(rng);
    case SimMode::BB2: return {SymId::SC1, SymId::SC2, SymId::SC3};
    case SimMode::Pot1: return {SymId::SC1};
    case SimMode::Pot2: return {SymId::SC2};
    case SimMode::Pot3: return {SymId::SC3};
    default: return {};
  }
}

static void printProgress(int pct, const Stats& st, double stake,
                          std::chrono::steady_clock::time_point t0) {
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - t0).count();
  double totalWon = st.baseWon + st.fsWon;
  double totalRtp = st.staked > 0 ? 100.0 * totalWon / st.staked : 0.0;
  double baseRtp = st.staked > 0 ? 100.0 * st.baseWon / st.staked : 0.0;
  double bonusRtp = st.staked > 0 ? 100.0 * st.fsWon / st.staked : 0.0;
  double hf = st.spins > 0 ? 100.0 * static_cast<double>(st.hits) / st.spins : 0.0;

  std::cout << std::fixed << std::setprecision(2)
            << "[" << std::setw(3) << pct << "%]"
            << "  elapsed=" << std::setprecision(1) << elapsed << "s"
            << "  spins=" << st.spins
            << std::setprecision(2)
            << "  Total RTP=" << totalRtp << "%"
            << "  Base RTP=" << baseRtp << "%"
            << "  Bonus RTP=" << bonusRtp << "%"
            << "  HF=" << hf << "%"
            << "  (stake/spin=" << stake << "×)\n"
            << std::flush;
}

SimReport runSim(const SimOptions& opts) {
  Rng rng(opts.seed);
  Stats st{};
  double stake = stakeForMode(opts.mode);
  int potState[3] = {0, 0, 0};

  auto t0 = std::chrono::steady_clock::now();
  const long long total = opts.spins;
  int nextPct = 10; // print at 10%, 20%, …, 100%

  auto maybeProgress = [&]() {
    if (total <= 0) return;
    int pct = static_cast<int>((100.0 * st.spins) / total);
    while (nextPct <= 100 && pct >= nextPct) {
      printProgress(nextPct, st, stake, t0);
      nextPct += 10;
    }
  };

  std::cout << "Sim start  mode=" << modeName(opts.mode)
            << "  spins=" << total
            << "  seed=" << opts.seed
            << "  stake/spin=" << stake << "×\n"
            << std::flush;

  if (opts.mode == SimMode::Main) {
    for (long long n = 0; n < total; ++n) {
      st.spins++;
      st.staked = round2(st.staked + stake);
      auto res = fastBaseRound(rng, potState, st);
      if (res.total > 0) st.hits++;
      recordPaid(st, res.total / BET);
      maybeProgress();
    }
  } else {
    for (long long n = 0; n < total; ++n) {
      st.spins++;
      st.staked = round2(st.staked + stake);
      potState[0] = potState[1] = potState[2] = 0;
      auto pots = potsForBuyMode(rng, opts.mode);
      for (SymId k : pots) potState[coinIndex(k)] = POT_NEED;
      bool active[3] = {false, false, false};
      for (SymId k : pots) active[coinIndex(k)] = true;
      auto triggerCoins = makeBuyTriggerCoins(rng, pots);
      st.fsTrig++;
      noteTierHit(active, st);
      double bonusWin = fastHoldWin(rng, triggerCoins, active, potState, st);
      st.fsWon = round2(st.fsWon + bonusWin);
      st.featSum = round2(st.featSum + bonusWin);
      double fx = bonusWin / BET;
      if (fx > st.biggestFeatX) st.biggestFeatX = fx;
      if (bonusWin > 0) st.hits++;
      recordPaid(st, bonusWin / BET);
      maybeProgress();
    }
  }

  // Ensure 100% line if total spins < 10 or rounding skipped it
  if (nextPct <= 100 && st.spins >= total)
    printProgress(100, st, stake, t0);

  auto t1 = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(t1 - t0).count();

  SimReport r;
  r.mode = opts.mode;
  r.spins = st.spins;
  r.stakePerSpin = stake;
  r.staked = st.staked;
  r.baseWon = st.baseWon;
  r.fsWon = st.fsWon;
  r.totalWon = round2(st.baseWon + st.fsWon);
  r.rtp = r.staked > 0 ? r.totalWon / r.staked : 0.0;
  r.meanX = st.spins > 0 ? (r.totalWon / BET) / st.spins : 0.0;
  r.hitRate = st.spins > 0 ? static_cast<double>(st.hits) / st.spins : 0.0;
  r.featureRate = st.spins > 0 ? static_cast<double>(st.fsTrig) / st.spins : 0.0;
  r.biggestFeatX = st.biggestFeatX;
  r.bestX = st.bestX;
  for (int i = 0; i < 3; ++i) r.potUse[i] = st.potUse[i];
  for (int i = 0; i < 4; ++i) r.tierHits[i] = st.tierHits[i];
  r.seconds = seconds;
  r.detail = st;
  return r;
}

static void appendHitWinRow(std::ostringstream& o, const char* label,
                            const HitWin& h, long long denomSpins, double staked) {
  double odds = denomSpins > 0 ? 100.0 * static_cast<double>(h.hits) / denomSpins : 0.0;
  double rtp = staked > 0 ? 100.0 * h.won / staked : 0.0;
  double avgX = h.hits > 0 ? (h.won / BET) / h.hits : 0.0;
  o << "  " << std::left << std::setw(28) << label << std::right
    << "  hits=" << std::setw(10) << h.hits
    << "  odds=" << std::setw(8) << std::setprecision(4) << odds << "%"
    << "  avg=" << std::setw(10) << std::setprecision(2) << avgX << "×"
    << "  RTP=" << std::setw(8) << std::setprecision(3) << rtp << "%\n";
}

std::string formatReport(const SimReport& r) {
  const Stats& st = r.detail;
  std::ostringstream o;
  o << std::fixed;
  o << "=== 3-Pot Hold & Win Sim — final ===\n";
  o << std::setprecision(2);
  o << "mode=" << modeName(r.mode)
    << "  spins=" << r.spins
    << "  stake/spin=" << r.stakePerSpin << "×\n";
  o << "staked=" << r.staked
    << "  won=" << r.totalWon
    << "  (base=" << r.baseWon << "  feature=" << r.fsWon << ")\n";
  double baseRtp = r.staked > 0 ? 100.0 * r.baseWon / r.staked : 0.0;
  double bonusRtp = r.staked > 0 ? 100.0 * r.fsWon / r.staked : 0.0;
  o << std::setprecision(3);
  o << "Total RTP=" << (100.0 * r.rtp) << "%"
    << "  Base RTP=" << baseRtp << "%"
    << "  Bonus RTP=" << bonusRtp << "%"
    << "  HF=" << (100.0 * r.hitRate) << "%"
    << "  feature=" << (100.0 * r.featureRate) << "%\n";
  o << "mean=" << r.meanX << "×\n";
  o << std::setprecision(2);
  o << "best=" << r.bestX << "×  biggestFeat=" << r.biggestFeatX << "×\n";
  o << "potUse SC1=" << r.potUse[0] << " SC2=" << r.potUse[1] << " SC3=" << r.potUse[2] << "\n";
  o << "tierHits 1=" << r.tierHits[1] << " 2=" << r.tierHits[2] << " 3=" << r.tierHits[3] << "\n";

  // --- Cumulative win thresholds ---
  o << "\n--- Win bands (cumulative thresholds) — odds / avg win / RTP ---\n";
  for (int i = 0; i < kCumBandCount; ++i)
    appendHitWinRow(o, cumBandName(i), st.cumBand[i], r.spins, r.staked);

  // --- Discrete win ranges ---
  o << "\n--- Win ranges (discrete) — odds / avg win / RTP ---\n";
  for (int i = 0; i < kWinRangeCount; ++i)
    appendHitWinRow(o, winRangeName(i), st.winRange[i], r.spins, r.staked);

  // --- Line symbol × OAK ---
  o << "\n--- Base lines (symbol × OAK) — odds per spin, RTP vs stake ---\n";
  o << std::setprecision(2);
  double lineRtp = r.staked > 0 ? 100.0 * st.lineWon / r.staked : 0.0;
  o << "  line total won=" << st.lineWon << "  RTP=" << std::setprecision(3) << lineRtp << "%\n";
  for (int si = 0; si < kPaySymCount; ++si) {
    for (int oi = 0; oi < kOakCount; ++oi) {
      const HitWin& h = st.lineSymOak[si][oi];
      if (h.hits == 0 && h.won == 0.0) continue;
      std::ostringstream label;
      label << paySymName(si) << " " << (oi + 3) << "oak";
      appendHitWinRow(o, label.str().c_str(), h, r.spins, r.staked);
    }
  }

  // --- Coins ---
  o << "\n--- Base coins — by count (odds = share of spins) ---\n";
  double coinRtp = r.staked > 0 ? 100.0 * st.coinWon / r.staked : 0.0;
  o << std::setprecision(2);
  o << "  coin total won=" << st.coinWon << "  RTP=" << std::setprecision(3) << coinRtp << "%\n";
  for (int n = 0; n <= 3; ++n) {
    std::ostringstream label;
    label << n << " coin" << (n == 1 ? "" : "s");
    appendHitWinRow(o, label.str().c_str(), st.coinByCount[n], r.spins, r.staked);
  }
  o << "--- Base coins — by face value (hits = lands; RTP includes combo multi share) ---\n";
  for (int vi = 0; vi < kCoinValueCount; ++vi) {
    std::ostringstream label;
    label << std::fixed << std::setprecision(1) << coinValueAt(vi) << "× value";
    appendHitWinRow(o, label.str().c_str(), st.coinByValue[vi], r.spins, r.staked);
  }
  o << "--- Base coins — by type ---\n";
  for (int ti = 0; ti < 3; ++ti) {
    std::ostringstream label;
    label << "SC" << (ti + 1);
    appendHitWinRow(o, label.str().c_str(), st.coinByType[ti], r.spins, r.staked);
  }

  // --- Bonus fill by pot ---
  o << "\n--- Bonus fill by starting pot (board locked-count bands) ---\n";
  o << "  (odds = boards in band / boards attributed to that starting pot;\n";
  o << "   RTP share split evenly across starting pots on multi-pot entries)\n";
  static const char* potNames[3] = {"Pot1 (SC1)", "Pot2 (SC2)", "Pot3 (SC3)"};
  for (int pi = 0; pi < 3; ++pi) {
    if (st.hwFeatByPot[pi] == 0 && st.hwBoardsByPot[pi] == 0) continue;
    o << "  " << potNames[pi]
      << "  features=" << st.hwFeatByPot[pi]
      << "  boards=" << st.hwBoardsByPot[pi] << "\n";
    long long boardDenom = st.hwBoardsByPot[pi] > 0 ? st.hwBoardsByPot[pi] : 1;
    for (int b = 0; b < kFillBandCount; ++b) {
      const HitWin& h = st.hwFillByPot[pi][b];
      double odds = 100.0 * static_cast<double>(h.hits) / boardDenom;
      double rtp = r.staked > 0 ? 100.0 * h.won / r.staked : 0.0;
      o << "    " << std::left << std::setw(22) << fillBandName(b) << std::right
        << "  hits=" << std::setw(8) << h.hits
        << "  odds=" << std::setw(7) << std::setprecision(2) << odds << "%"
        << "  RTP=" << std::setw(7) << std::setprecision(3) << rtp << "%\n";
    }
  }

  // --- Jackpots ---
  o << "\n--- Bonus jackpots (hits = locked JP cells; GRAND also counts full-board awards) ---\n";
  long long featDenom = st.fsTrig > 0 ? st.fsTrig : r.spins;
  for (int ji = 0; ji < kJpCount; ++ji) {
    const HitWin& h = st.jp[ji];
    double oddsPerFeat = featDenom > 0 ? 100.0 * static_cast<double>(h.hits) / featDenom : 0.0;
    double oddsPerSpin = r.spins > 0 ? 100.0 * static_cast<double>(h.hits) / r.spins : 0.0;
    double rtp = r.staked > 0 ? 100.0 * h.won / r.staked : 0.0;
    o << "  " << std::left << std::setw(8) << jpName(ji) << std::right
      << "  hits=" << std::setw(8) << h.hits
      << "  odds/feat=" << std::setw(8) << std::setprecision(3) << oddsPerFeat << "%"
      << "  odds/spin=" << std::setw(8) << oddsPerSpin << "%"
      << "  RTP=" << std::setw(7) << rtp << "%\n";
  }

  o << "\n" << std::setprecision(3);
  o << "runtime=" << r.seconds << "s"
    << "  (" << (r.seconds > 0 ? r.spins / r.seconds : 0) << " spins/s)\n";
  return o.str();
}

} // namespace threepot
