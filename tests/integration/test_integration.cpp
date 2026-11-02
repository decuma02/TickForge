#include <gtest/gtest.h>
#include "tickforge/engine.hpp"
#include <random>

TEST(EngineIntegrationTest, HighVolumeIngestionPipeline) {
    tickforge::MatchingEngine engine;

    constexpr size_t NUM_TICKS = 10000;
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(50.0, 500.0);
    std::uniform_int_distribution<uint32_t> vol_dist(1, 1000);

    for (size_t i = 0; i < NUM_TICKS; ++i) {
        tickforge::Tick tick{
            static_cast<uint64_t>(1000000 + i),
            1,
            price_dist(rng),
            vol_dist(rng)
        };
        engine.process_tick(tick);
    }

    EXPECT_EQ(engine.get_total_ticks(), NUM_TICKS);
    EXPECT_GT(engine.compute_vwap(), 50.0);
    EXPECT_LT(engine.compute_vwap(), 500.0);
}
