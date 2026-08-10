#include "sim.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace threepot;

static void usage(const char* argv0) {
  std::cerr
    << "Usage: " << argv0 << " [--mode main|bb1|bb2|pot1|pot2|pot3] [--spins N] [--seed S]\n"
    << "  --mode   Simulation mode (default: main)\n"
    << "  --spins  Number of rounds (default: 100000)\n"
    << "  --seed   RNG seed (default: 1)\n";
}

static bool parseMode(const std::string& s, SimMode& out) {
  if (s == "main") { out = SimMode::Main; return true; }
  if (s == "bb1") { out = SimMode::BB1; return true; }
  if (s == "bb2") { out = SimMode::BB2; return true; }
  if (s == "pot1") { out = SimMode::Pot1; return true; }
  if (s == "pot2") { out = SimMode::Pot2; return true; }
  if (s == "pot3") { out = SimMode::Pot3; return true; }
  return false;
}

int main(int argc, char** argv) {
  SimOptions opts;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        usage(argv[0]);
        std::exit(1);
      }
      return argv[++i];
    };
    if (a == "--mode") {
      if (!parseMode(need("--mode"), opts.mode)) {
        std::cerr << "Unknown mode\n";
        usage(argv[0]);
        return 1;
      }
    } else if (a == "--spins") {
      opts.spins = std::stoll(need("--spins"));
    } else if (a == "--seed") {
      opts.seed = static_cast<std::uint64_t>(std::stoull(need("--seed")));
    } else if (a == "-h" || a == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown arg: " << a << "\n";
      usage(argv[0]);
      return 1;
    }
  }

  if (opts.spins <= 0) {
    std::cerr << "--spins must be > 0\n";
    return 1;
  }

  SimReport report = runSim(opts);
  std::cout << formatReport(report);
  return 0;
}
