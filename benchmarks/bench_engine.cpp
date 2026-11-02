#include <benchmark/benchmark.h>
#include "tickforge/engine.hpp"

static void BM_EngineProcessTick(benchmark::State& state) {
    tickforge::MatchingEngine engine;
    tickforge::Tick tick{1672531200000000000ULL, 1, 102.5, 100};

    for (auto _ : state) {
        engine.process_tick(tick);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EngineProcessTick);

static void BM_EngineComputeVWAP(benchmark::State& state) {
    tickforge::MatchingEngine engine;
    for (int i = 0; i < 5000; ++i) {
        engine.process_tick({static_cast<uint64_t>(i), 1, 100.0 + (i % 10), 10});
    }

    for (auto _ : state) {
        double vwap = engine.compute_vwap();
        benchmark::DoNotOptimize(vwap);
    }
}
BENCHMARK(BM_EngineComputeVWAP);

BENCHMARK_MAIN();
