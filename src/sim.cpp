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

  // Progress goes to stderr so stdout CSV stays clean for redirects.
  std::cerr << std::fixed << std::setprecision(2)
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

  std::cerr << "Sim start  mode=" << modeName(opts.mode)
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

static void csvEscape(std::ostringstream& o, const std::string& s) {
  bool needQuotes = s.find_first_of(",\"\n\r") != std::string::npos;
  if (!needQuotes) {
    o << s;
    return;
  }
  o << '"';
  for (char c : s) {
    if (c == '"') o << "\"\"";
    else o << c;
  }
  o << '"';
}

static void appendCsvRow(std::ostringstream& o, const char* section, const std::string& label,
                         long long hits, double oddsPct, double avgX, double rtpPct) {
  o << section << ',';
  csvEscape(o, label);
  o << ',' << hits
    << ',' << std::fixed << std::setprecision(6) << oddsPct
    << ',' << std::setprecision(6) << avgX
    << ',' << std::setprecision(6) << rtpPct
    << '\n';
}

static void appendHitWinCsv(std::ostringstream& o, const char* section, const char* label,
                            const HitWin& h, long long denomSpins, double staked) {
  double odds = denomSpins > 0 ? 100.0 * static_cast<double>(h.hits) / denomSpins : 0.0;
  double rtp = staked > 0 ? 100.0 * h.won / staked : 0.0;
  double avgX = h.hits > 0 ? (h.won / BET) / h.hits : 0.0;
  appendCsvRow(o, section, label, h.hits, odds, avgX, rtp);
}

std::string formatReport(const SimReport& r) {
  const Stats& st = r.detail;
  std::ostringstream out;
  out << "section,label,hits,odds_pct,avg_x,rtp_pct\n";

  double baseRtp = r.staked > 0 ? 100.0 * r.baseWon / r.staked : 0.0;
  double bonusRtp = r.staked > 0 ? 100.0 * r.fsWon / r.staked : 0.0;

  appendCsvRow(out, "summary", std::string("mode=") + modeName(r.mode), r.spins, 0.0, r.stakePerSpin, 100.0 * r.rtp);
  appendCsvRow(out, "summary", "spins", r.spins, 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "stake_per_spin", 0, 0.0, r.stakePerSpin, 0.0);
  appendCsvRow(out, "summary", "staked", 0, 0.0, r.staked, 0.0);
  appendCsvRow(out, "summary", "total_won", 0, 0.0, r.totalWon, 0.0);
  appendCsvRow(out, "summary", "base_won", 0, 0.0, r.baseWon, 0.0);
  appendCsvRow(out, "summary", "feature_won", 0, 0.0, r.fsWon, 0.0);
  appendCsvRow(out, "summary", "total_rtp_pct", 0, 0.0, 0.0, 100.0 * r.rtp);
  appendCsvRow(out, "summary", "base_rtp_pct", 0, 0.0, 0.0, baseRtp);
  appendCsvRow(out, "summary", "bonus_rtp_pct", 0, 0.0, 0.0, bonusRtp);
  appendCsvRow(out, "summary", "hit_freq_pct", 0, 100.0 * r.hitRate, 0.0, 0.0);
  appendCsvRow(out, "summary", "feature_rate_pct", 0, 100.0 * r.featureRate, 0.0, 0.0);
  appendCsvRow(out, "summary", "mean_x", 0, 0.0, r.meanX, 0.0);
  appendCsvRow(out, "summary", "best_x", 0, 0.0, r.bestX, 0.0);
  appendCsvRow(out, "summary", "biggest_feat_x", 0, 0.0, r.biggestFeatX, 0.0);
  appendCsvRow(out, "summary", "pot_use_SC1", r.potUse[0], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "pot_use_SC2", r.potUse[1], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "pot_use_SC3", r.potUse[2], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "tier_hits_1", r.tierHits[1], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "tier_hits_2", r.tierHits[2], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "tier_hits_3", r.tierHits[3], 0.0, 0.0, 0.0);
  appendCsvRow(out, "summary", "runtime_s", 0, 0.0, r.seconds, 0.0);
  appendCsvRow(out, "summary", "spins_per_s", 0, 0.0, r.seconds > 0 ? r.spins / r.seconds : 0.0, 0.0);

  for (int i = 0; i < kCumBandCount; ++i)
    appendHitWinCsv(out, "win_cum", cumBandName(i), st.cumBand[i], r.spins, r.staked);

  for (int i = 0; i < kWinRangeCount; ++i)
    appendHitWinCsv(out, "win_range", winRangeName(i), st.winRange[i], r.spins, r.staked);

  for (int si = 0; si < kPaySymCount; ++si) {
    for (int oi = 0; oi < kOakCount; ++oi) {
      std::ostringstream label;
      label << paySymName(si) << " " << (oi + 3) << "oak";
      appendHitWinCsv(out, "line", label.str().c_str(), st.lineSymOak[si][oi], r.spins, r.staked);
    }
  }
  appendCsvRow(out, "line", "line_total", 0, 0.0, st.lineWon,
               r.staked > 0 ? 100.0 * st.lineWon / r.staked : 0.0);

  for (int n = 0; n <= 3; ++n) {
    std::ostringstream label;
    label << n << " coin" << (n == 1 ? "" : "s");
    appendHitWinCsv(out, "coin_count", label.str().c_str(), st.coinByCount[n], r.spins, r.staked);
  }
  for (int vi = 0; vi < kCoinValueCount; ++vi) {
    std::ostringstream label;
    label << std::fixed << std::setprecision(1) << coinValueAt(vi) << "x value";
    appendHitWinCsv(out, "coin_value", label.str().c_str(), st.coinByValue[vi], r.spins, r.staked);
  }
  for (int ti = 0; ti < 3; ++ti) {
    std::ostringstream label;
    label << "SC" << (ti + 1);
    appendHitWinCsv(out, "coin_type", label.str().c_str(), st.coinByType[ti], r.spins, r.staked);
  }
  appendCsvRow(out, "coin_count", "coin_total", 0, 0.0, st.coinWon,
               r.staked > 0 ? 100.0 * st.coinWon / r.staked : 0.0);

  static const char* potNames[3] = {"Pot1_SC1", "Pot2_SC2", "Pot3_SC3"};
  for (int pi = 0; pi < 3; ++pi) {
    if (st.hwFeatByPot[pi] == 0 && st.hwBoardsByPot[pi] == 0) continue;
    appendCsvRow(out, "hw_fill", std::string(potNames[pi]) + "_features",
                 st.hwFeatByPot[pi], 0.0, 0.0, 0.0);
    appendCsvRow(out, "hw_fill", std::string(potNames[pi]) + "_boards",
                 st.hwBoardsByPot[pi], 0.0, 0.0, 0.0);
    long long boardDenom = st.hwBoardsByPot[pi] > 0 ? st.hwBoardsByPot[pi] : 1;
    for (int b = 0; b < kFillBandCount; ++b) {
      std::ostringstream label;
      label << potNames[pi] << "|" << fillBandName(b);
      appendHitWinCsv(out, "hw_fill", label.str().c_str(), st.hwFillByPot[pi][b],
                      boardDenom, r.staked);
    }
  }

  long long featDenom = st.fsTrig > 0 ? st.fsTrig : r.spins;
  for (int ji = 0; ji < kJpCount; ++ji) {
    const HitWin& h = st.jp[ji];
    double oddsPerSpin = r.spins > 0 ? 100.0 * static_cast<double>(h.hits) / r.spins : 0.0;
    double oddsPerFeat = featDenom > 0 ? 100.0 * static_cast<double>(h.hits) / featDenom : 0.0;
    double rtp = r.staked > 0 ? 100.0 * h.won / r.staked : 0.0;
    // avg_x column carries odds_per_feat_pct for jackpots
    appendCsvRow(out, "jp", jpName(ji), h.hits, oddsPerSpin, oddsPerFeat, rtp);
  }

  return out.str();
}

} // namespace threepot
