#include "rng.hpp"

namespace threepot {

Rng::Rng(std::uint64_t seed) : eng_(seed) {}

void Rng::reseed(std::uint64_t seed) { eng_.seed(seed); }

double Rng::nextDouble() { return uni_(eng_); }

bool Rng::chance(double p) {
  if (p <= 0.0) return false;
  if (p >= 1.0) return true;
  return nextDouble() < p;
}

int Rng::nextInt(int n) {
  if (n <= 1) return 0;
  return static_cast<int>(nextDouble() * n);
}

int Rng::weightedIndex(const std::vector<double>& weights) {
  double tot = 0.0;
  for (double w : weights) tot += w;
  double r = nextDouble() * tot;
  double acc = 0.0;
  for (size_t i = 0; i < weights.size(); ++i) {
    acc += weights[i];
    if (r < acc) return static_cast<int>(i);
  }
  return static_cast<int>(weights.size()) - 1;
}

} // namespace threepot
