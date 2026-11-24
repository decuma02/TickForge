#pragma once

#include <cstdint>
#include <string>
#include <limits>
#include <stdexcept>
#include <ostream>

namespace tickforge::domain {

/// Strongly-typed discrete price in integer ticks.
/// For CME Globex ES outright futures:
/// 1 tick = 0.25 index points ($12.50 per contract on a $50 multiplier).
class PriceInTicks {
public:
    constexpr PriceInTicks() noexcept : ticks_(0) {}

    /// Construct from raw integer ticks. Validates that ticks is strictly positive (> 0).
    /// Does not silently clamp or saturate.
    constexpr explicit PriceInTicks(int64_t ticks) : ticks_(ticks) {
        if (ticks <= 0) {
            throw std::invalid_argument("PriceInTicks must be strictly positive (> 0)");
        }
    }

    /// Factory for sentinel / unassigned / zero price.
    [[nodiscard]] static constexpr PriceInTicks zero() noexcept {
        PriceInTicks p;
        p.ticks_ = 0;
        return p;
    }

    /// Construct from raw ticks allowing zero (for internal sentinel/zero arithmetic).
    [[nodiscard]] static constexpr PriceInTicks from_raw(int64_t ticks) {
        if (ticks < 0) {
            throw std::invalid_argument("PriceInTicks cannot be negative");
        }
        PriceInTicks p;
        p.ticks_ = ticks;
        return p;
    }

    /// Create PriceInTicks from index point units (e.g. 5000.25) given tick size in index points (e.g. 0.25).
    /// Ensures exact integer divisibility; throws if fractional tick remainder exists.
    [[nodiscard]] static PriceInTicks from_index_points(int64_t whole_points, uint32_t fractional_points_e4, uint32_t tick_size_e4) {
        if (tick_size_e4 == 0) {
            throw std::invalid_argument("Tick size cannot be zero");
        }
        int64_t total_points_e4 = (whole_points * 10'000) + static_cast<int64_t>(fractional_points_e4);
        if (total_points_e4 <= 0) {
            throw std::invalid_argument("Total price points must be positive");
        }
        if (total_points_e4 % static_cast<int64_t>(tick_size_e4) != 0) {
            throw std::invalid_argument("Price is not an exact multiple of the minimum tick size");
        }
        int64_t ticks = total_points_e4 / static_cast<int64_t>(tick_size_e4);
        return PriceInTicks(ticks);
    }

    [[nodiscard]] constexpr int64_t ticks() const noexcept { return ticks_; }

    [[nodiscard]] constexpr bool operator==(const PriceInTicks& other) const noexcept { return ticks_ == other.ticks_; }
    [[nodiscard]] constexpr bool operator!=(const PriceInTicks& other) const noexcept { return ticks_ != other.ticks_; }
    [[nodiscard]] constexpr bool operator<(const PriceInTicks& other) const noexcept { return ticks_ < other.ticks_; }
    [[nodiscard]] constexpr bool operator<=(const PriceInTicks& other) const noexcept { return ticks_ <= other.ticks_; }
    [[nodiscard]] constexpr bool operator>(const PriceInTicks& other) const noexcept { return ticks_ > other.ticks_; }
    [[nodiscard]] constexpr bool operator>=(const PriceInTicks& other) const noexcept { return ticks_ >= other.ticks_; }

    [[nodiscard]] constexpr PriceInTicks operator+(const PriceInTicks& other) const {
        if (other.ticks_ > std::numeric_limits<int64_t>::max() - ticks_) {
            throw std::overflow_error("PriceInTicks addition overflow");
        }
        return from_raw(ticks_ + other.ticks_);
    }

    [[nodiscard]] constexpr PriceInTicks operator-(const PriceInTicks& other) const {
        if (ticks_ < other.ticks_) {
            throw std::underflow_error("PriceInTicks subtraction underflow (result would be negative)");
        }
        return from_raw(ticks_ - other.ticks_);
    }

    [[nodiscard]] std::string to_string() const {
        return std::to_string(ticks_) + " ticks";
    }

private:
    int64_t ticks_;
};

inline std::ostream& operator<<(std::ostream& os, const PriceInTicks& price) {
    return os << price.ticks() << "t";
}

} // namespace tickforge::domain

namespace std {
template <>
struct hash<tickforge::domain::PriceInTicks> {
    size_t operator()(const tickforge::domain::PriceInTicks& p) const noexcept {
        return hash<int64_t>{}(p.ticks());
    }
};
} // namespace std
