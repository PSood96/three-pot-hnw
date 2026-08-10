#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace threepot {

class Rng {
public:
  explicit Rng(std::uint64_t seed = 1);

  void reseed(std::uint64_t seed);
  double nextDouble(); // [0,1)
  bool chance(double p);
  int nextInt(int n); // [0, n)
  int weightedIndex(const std::vector<double>& weights);

  template <typename T>
  T pickWeighted(const std::vector<std::pair<T, double>>& items) {
    double tot = 0.0;
    for (const auto& it : items) tot += it.second;
    double r = nextDouble() * tot;
    double acc = 0.0;
    for (const auto& it : items) {
      acc += it.second;
      if (r < acc) return it.first;
    }
    return items.back().first;
  }

  template <typename T>
  void shuffle(std::vector<T>& a) {
    for (int i = static_cast<int>(a.size()) - 1; i > 0; --i) {
      int j = nextInt(i + 1);
      std::swap(a[i], a[j]);
    }
  }

private:
  std::mt19937_64 eng_;
  std::uniform_real_distribution<double> uni_{0.0, 1.0};
};

} // namespace threepot
