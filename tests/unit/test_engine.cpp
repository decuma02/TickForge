#include <gtest/gtest.h>
#include "tickforge/engine.hpp"

class EngineUnitTest : public ::testing::Test {
protected:
    tickforge::MatchingEngine engine;
};

TEST_F(EngineUnitTest, InitiallyEmpty) {
    EXPECT_EQ(engine.get_total_ticks(), 0);
    EXPECT_DOUBLE_EQ(engine.get_last_price(), 0.0);
    EXPECT_DOUBLE_EQ(engine.compute_vwap(), 0.0);
}

TEST_F(EngineUnitTest, ProcessSingleTick) {
    tickforge::Tick t{1000, 1, 150.25, 20};
    engine.process_tick(t);

    EXPECT_EQ(engine.get_total_ticks(), 1);
    EXPECT_DOUBLE_EQ(engine.get_last_price(), 150.25);
    EXPECT_DOUBLE_EQ(engine.compute_vwap(), 150.25);
}

TEST_F(EngineUnitTest, ComputeCorrectVWAP) {
    // Tick 1: 100 shares @ $10.0 = $1,000
    engine.process_tick({1001, 1, 10.0, 100});
    // Tick 2: 300 shares @ $20.0 = $6,000
    engine.process_tick({1002, 1, 20.0, 300});
    // Total turnover = $7,000, Total shares = 400. VWAP = 7000 / 400 = 17.5

    EXPECT_EQ(engine.get_total_ticks(), 2);
    EXPECT_DOUBLE_EQ(engine.get_last_price(), 20.0);
    EXPECT_DOUBLE_EQ(engine.compute_vwap(), 17.5);
}
