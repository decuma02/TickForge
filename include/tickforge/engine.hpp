#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tickforge {

struct Tick {
    uint64_t timestamp_ns;
    uint32_t symbol_id;
    double price;
    uint32_t volume;
};

class MatchingEngine {
public:
    MatchingEngine();
    ~MatchingEngine() = default;

    void process_tick(const Tick& tick);
    [[nodiscard]] size_t get_total_ticks() const noexcept;
    [[nodiscard]] double get_last_price() const noexcept;
    [[nodiscard]] double compute_vwap() const noexcept;

private:
    std::vector<Tick> ticks_;
    double cumulative_turnover_{0.0};
    uint64_t cumulative_volume_{0};
};

} // namespace tickforge
