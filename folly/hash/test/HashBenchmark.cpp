/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <folly/hash/Hash.h>
#include <folly/hash/MurmurHash.h>
#include <folly/hash/rapidhash.h>

#include <stdint.h>

#include <deque>
#include <random>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <glog/logging.h>

#include <folly/Benchmark.h>
#include <folly/Preprocessor.h>
#include <folly/lang/Keep.h>
#include <folly/portability/GFlags.h>

extern "C" FOLLY_KEEP uint64_t
check_folly_spooky_hash_v2_hash_32(void const* data, size_t size) {
  return folly::hash::SpookyHashV2::Hash32(data, size, 0);
}

extern "C" FOLLY_KEEP uint64_t
check_folly_spooky_hash_v2_hash_64(void const* data, size_t size) {
  return folly::hash::SpookyHashV2::Hash64(data, size, 0);
}

namespace detail {

std::vector<uint8_t> randomBytes(size_t n) {
  std::vector<uint8_t> ret(n);
  std::default_random_engine rng(1729); // Deterministic seed.
  std::uniform_int_distribution<uint16_t> dist(0, 255);
  std::generate(ret.begin(), ret.end(), [&]() { return dist(rng); });
  return ret;
}

std::vector<uint8_t> benchData = randomBytes(1 << 20); // 1MiB, fits in cache.

template <class Hasher>
void bmHasher(Hasher hasher, size_t k, size_t iters) {
  CHECK_LE(k, benchData.size());
  for (size_t i = 0, pos = 0; i < iters; ++i, ++pos) {
    if (pos == benchData.size() - k + 1) {
      pos = 0;
    }
    folly::doNotOptimizeAway(hasher(benchData.data() + pos, k));
  }
}

template <class Hasher>
void addHashBenchmark(const std::string& name) {
  static std::deque<std::string> names;

  for (size_t k = 1; k < 16; ++k) {
    names.emplace_back(fmt::format("{}: k={}", name, k));
    folly::addBenchmark(__FILE__, names.back().c_str(), [=](unsigned iters) {
      Hasher hasher;
      bmHasher(hasher, k, iters);
      return iters;
    });
  }

  for (size_t i = 0; i < 16; ++i) {
    auto k = size_t(1) << i;
    names.emplace_back(fmt::format("{}: k=2^{}", name, i));
    folly::addBenchmark(__FILE__, names.back().c_str(), [=](unsigned iters) {
      Hasher hasher;
      bmHasher(hasher, k, iters);
      return iters;
    });
  }

  /* Draw line. */
  folly::addBenchmark(__FILE__, "-", []() { return 0; });
}

struct SpookyHashV2 {
  uint64_t operator()(const uint8_t* data, size_t size) const {
    return folly::hash::SpookyHashV2::Hash64(data, size, 0);
  }
};

struct FNV64 {
  uint64_t operator()(const uint8_t* data, size_t size) const {
    return folly::hash::fnv64_buf(data, size);
  }
};

struct MurmurHash {
  uint64_t operator()(const uint8_t* data, size_t size) const {
    return folly::hash::murmurHash64(
        reinterpret_cast<const char*>(data), size, 0);
  }
};

struct RapidHash {
  uint64_t operator()(const uint8_t* data, size_t size) const {
    return folly::hash::rapidhash(
        reinterpret_cast<const void*>(data), size);
  }
};

} // namespace detail

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  std::deque<std::string> names; // Backing for benchmark names.

#define BENCHMARK_HASH(HASHER) \
  detail::addHashBenchmark<detail::HASHER>(FOLLY_PP_STRINGIZE(HASHER));

  BENCHMARK_HASH(SpookyHashV2);
  BENCHMARK_HASH(FNV64);
  BENCHMARK_HASH(MurmurHash);
  BENCHMARK_HASH(RapidHash);

#undef BENCHMARK_HASH

  folly::runBenchmarks();

  return 0;
}

#if 0
AMD Ryzen 9 5900X CPU @ 4.65GHz
$ hash_benchmark --bm_min_usec=100000
============================================================================
fbcode/folly/hash/test/HashBenchmark.cpp     relative  time/iter   iters/s
============================================================================
SpookyHashV2: k=1                                           5.13ns   195.06M
SpookyHashV2: k=2                                           5.35ns   187.06M
SpookyHashV2: k=3                                           5.37ns   186.26M
SpookyHashV2: k=4                                           5.14ns   194.62M
SpookyHashV2: k=5                                           5.36ns   186.53M
SpookyHashV2: k=6                                           5.38ns   185.95M
SpookyHashV2: k=7                                           5.58ns   179.15M
SpookyHashV2: k=8                                           5.16ns   193.94M
SpookyHashV2: k=9                                           5.17ns   193.57M
SpookyHashV2: k=10                                          5.37ns   186.07M
SpookyHashV2: k=11                                          5.39ns   185.42M
SpookyHashV2: k=12                                          5.17ns   193.40M
SpookyHashV2: k=13                                          5.38ns   185.90M
SpookyHashV2: k=14                                          5.39ns   185.49M
SpookyHashV2: k=15                                          5.59ns   178.82M
SpookyHashV2: k=2^0                                         5.16ns   193.88M
SpookyHashV2: k=2^1                                         5.38ns   186.04M
SpookyHashV2: k=2^2                                         5.16ns   193.81M
SpookyHashV2: k=2^3                                         5.17ns   193.58M
SpookyHashV2: k=2^4                                         8.11ns   123.31M
SpookyHashV2: k=2^5                                         8.36ns   119.67M
SpookyHashV2: k=2^6                                        13.29ns    75.27M
SpookyHashV2: k=2^7                                        22.36ns    44.73M
SpookyHashV2: k=2^8                                        38.10ns    26.25M
SpookyHashV2: k=2^9                                        49.01ns    20.41M
SpookyHashV2: k=2^10                                       70.40ns    14.21M
SpookyHashV2: k=2^11                                      114.81ns     8.71M
SpookyHashV2: k=2^12                                      202.86ns     4.93M
SpookyHashV2: k=2^13                                      379.74ns     2.63M
SpookyHashV2: k=2^14                                      735.19ns     1.36M
SpookyHashV2: k=2^15                                        1.44us   692.20K
----------------------------------------------------------------------------
FNV64: k=1                                                  1.17ns   856.15M
FNV64: k=2                                                  1.92ns   521.10M
FNV64: k=3                                                  2.82ns   355.12M
FNV64: k=4                                                  3.81ns   262.36M
FNV64: k=5                                                  4.73ns   211.58M
FNV64: k=6                                                  5.88ns   170.07M
FNV64: k=7                                                  7.11ns   140.61M
FNV64: k=8                                                  8.07ns   123.99M
FNV64: k=9                                                  9.54ns   104.83M
FNV64: k=10                                                10.98ns    91.09M
FNV64: k=11                                                12.48ns    80.10M
FNV64: k=12                                                13.45ns    74.37M
FNV64: k=13                                                14.40ns    69.47M
FNV64: k=14                                                15.88ns    62.98M
FNV64: k=15                                                17.37ns    57.56M
FNV64: k=2^0                                                1.17ns   854.73M
FNV64: k=2^1                                                1.92ns   520.85M
FNV64: k=2^2                                                3.81ns   262.24M
FNV64: k=2^3                                                8.08ns   123.83M
FNV64: k=2^4                                               19.03ns    52.54M
FNV64: k=2^5                                               42.42ns    23.58M
FNV64: k=2^6                                               89.76ns    11.14M
FNV64: k=2^7                                              186.25ns     5.37M
FNV64: k=2^8                                              375.03ns     2.67M
FNV64: k=2^9                                              752.83ns     1.33M
FNV64: k=2^10                                               1.51us   662.80K
FNV64: k=2^11                                               3.02us   331.01K
FNV64: k=2^12                                               6.04us   165.44K
FNV64: k=2^13                                              12.09us    82.70K
FNV64: k=2^14                                              24.19us    41.34K
FNV64: k=2^15                                              48.38us    20.67K
----------------------------------------------------------------------------
MurmurHash: k=1                                             1.49ns   669.53M
MurmurHash: k=2                                             1.50ns   668.14M
MurmurHash: k=3                                             1.40ns   715.14M
MurmurHash: k=4                                             1.49ns   669.40M
MurmurHash: k=5                                             1.41ns   710.20M
MurmurHash: k=6                                             1.51ns   662.78M
MurmurHash: k=7                                             1.44ns   694.43M
MurmurHash: k=8                                             1.79ns   559.74M
MurmurHash: k=9                                             2.14ns   467.71M
MurmurHash: k=10                                            2.08ns   480.64M
MurmurHash: k=11                                            2.32ns   431.27M
MurmurHash: k=12                                            2.11ns   474.60M
MurmurHash: k=13                                            2.14ns   466.30M
MurmurHash: k=14                                            2.13ns   469.61M
MurmurHash: k=15                                            2.25ns   444.42M
MurmurHash: k=2^0                                           1.49ns   669.56M
MurmurHash: k=2^1                                           1.50ns   666.43M
MurmurHash: k=2^2                                           1.50ns   668.27M
MurmurHash: k=2^3                                           1.79ns   559.17M
MurmurHash: k=2^4                                           2.41ns   415.72M
MurmurHash: k=2^5                                           3.84ns   260.43M
MurmurHash: k=2^6                                           6.99ns   143.15M
MurmurHash: k=2^7                                          12.08ns    82.79M
MurmurHash: k=2^8                                          23.45ns    42.64M
MurmurHash: k=2^9                                          49.96ns    20.01M
MurmurHash: k=2^10                                        107.91ns     9.27M
MurmurHash: k=2^11                                        218.65ns     4.57M
MurmurHash: k=2^12                                        441.28ns     2.27M
MurmurHash: k=2^13                                        886.75ns     1.13M
MurmurHash: k=2^14                                          1.77us   564.30K
MurmurHash: k=2^15                                          3.56us   280.85K
----------------------------------------------------------------------------
RapidHash: k=1                                              1.84ns   542.90M
RapidHash: k=2                                              1.82ns   549.15M
RapidHash: k=3                                              1.82ns   549.21M
RapidHash: k=4                                              1.57ns   638.11M
RapidHash: k=5                                              1.58ns   632.71M
RapidHash: k=6                                              1.58ns   633.68M
RapidHash: k=7                                              1.58ns   634.58M
RapidHash: k=8                                              1.58ns   634.57M
RapidHash: k=9                                              1.57ns   635.57M
RapidHash: k=10                                             1.58ns   632.12M
RapidHash: k=11                                             1.57ns   636.44M
RapidHash: k=12                                             1.57ns   636.24M
RapidHash: k=13                                             1.58ns   634.89M
RapidHash: k=14                                             1.58ns   634.86M
RapidHash: k=15                                             1.57ns   635.39M
RapidHash: k=2^0                                            1.84ns   542.43M
RapidHash: k=2^1                                            1.83ns   547.56M
RapidHash: k=2^2                                            1.57ns   637.86M
RapidHash: k=2^3                                            1.58ns   634.29M
RapidHash: k=2^4                                            1.58ns   633.76M
RapidHash: k=2^5                                            2.18ns   458.28M
RapidHash: k=2^6                                            3.52ns   284.33M
RapidHash: k=2^7                                            5.89ns   169.89M
RapidHash: k=2^8                                            8.92ns   112.14M
RapidHash: k=2^9                                           17.06ns    58.63M
RapidHash: k=2^10                                          31.49ns    31.76M
RapidHash: k=2^11                                          61.36ns    16.30M
RapidHash: k=2^12                                         120.33ns     8.31M
RapidHash: k=2^13                                         238.81ns     4.19M
RapidHash: k=2^14                                         511.88ns     1.95M
RapidHash: k=2^15                                         967.88ns     1.03M
----------------------------------------------------------------------------
#endif
