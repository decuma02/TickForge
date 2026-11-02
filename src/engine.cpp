#include "tickforge/engine.hpp"
#include <numeric>

namespace tickforge {

MatchingEngine::MatchingEngine() {
    ticks_.reserve(10000);
}

void MatchingEngine::process_tick(const Tick& tick) {
    ticks_.push_back(tick);
    cumulative_turnover_ += tick.price * static_cast<double>(tick.volume);
    cumulative_volume_ += tick.volume;
}

size_t MatchingEngine::get_total_ticks() const noexcept {
    return ticks_.size();
}

double MatchingEngine::get_last_price() const noexcept {
    if (ticks_.empty()) {
        return 0.0;
    }
    return ticks_.back().price;
}

double MatchingEngine::compute_vwap() const noexcept {
    if (cumulative_volume_ == 0) {
        return 0.0;
    }
    return cumulative_turnover_ / static_cast<double>(cumulative_volume_);
}

} // namespace tickforge
