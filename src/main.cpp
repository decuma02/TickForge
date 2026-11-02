#include "tickforge/engine.hpp"
#include <iostream>

int main() {
    std::cout << "[TickForge] Initializing engine..." << std::endl;
    tickforge::MatchingEngine engine;

    engine.process_tick({1672531199000000000ULL, 1, 100.5, 50});
    engine.process_tick({1672531200000000000ULL, 1, 101.0, 150});

    std::cout << "Processed Ticks : " << engine.get_total_ticks() << std::endl;
    std::cout << "Last Price      : " << engine.get_last_price() << std::endl;
    std::cout << "VWAP            : " << engine.compute_vwap() << std::endl;

    return 0;
}
