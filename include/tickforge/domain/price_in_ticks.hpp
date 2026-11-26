#pragma once

#include <cstdint>
#include <string>
#include <limits>
#include <stdexcept>
#include <ostream>
#include <cmath>

namespace tickforge::domain {

/// Strongly-typed discrete price in integer ticks.
///
/// Note on Sign:
/// Mathematically, an integer tick representation supports negative, zero, and positive ticks.
/// In general financial markets, certain contracts (e.g. WTI crude oil futures, power, options spreads)
/// can trade at zero or negative prices.
/// Therefore, PriceInTicks as a general integer-tick representation allows arbitrary 64-bit signed ticks
/// and enforces overflow/underflow safety.
/// Instrument-specific constraints (e.g. ES futures requiring strictly positive price > 0)
/// are validated by ProductSpecification and Order validation, rather than hardcoded as an intrinsic
/// restriction of general discrete tick arithmetic.
class PriceInTicks {
public:
    constexpr PriceInTicks() noexcept : ticks_(0) {}

    /// Construct from raw signed integer ticks.
    constexpr explicit PriceInTicks(int64_t ticks) noexcept : ticks_(ticks) {}

    /// Sentinel or zero price.
    [[nodiscard]] static constexpr PriceInTicks zero() noexcept {
        return PriceInTicks(0);
    }

    /// Factory for raw ticks (retained for backward compatibility / explicit call sites).
    [[nodiscard]] static constexpr PriceInTicks from_raw(int64_t ticks) noexcept {
        return PriceInTicks(ticks);
    }

    /// Create PriceInTicks from index point units (e.g. 5000.25) given tick size in index points (e.g. 0.25).
    /// Ensures exact integer divisibility; throws if fractional tick remainder exists.
    [[nodiscard]] static PriceInTicks from_index_points(int64_t whole_points, uint32_t fractional_points_e4, uint32_t tick_size_e4) {
        if (tick_size_e4 == 0) {
            throw std::invalid_argument("Tick size cannot be zero");
        }
        if (fractional_points_e4 >= 10'000) {
            throw std::invalid_argument("Fractional points e4 must be less than 10,000");
        }

        // Check for multiplication overflow
        constexpr int64_t max_whole = std::numeric_limits<int64_t>::max() / 10'000;
        constexpr int64_t min_whole = std::numeric_limits<int64_t>::min() / 10'000;
        if (whole_points > max_whole || whole_points < min_whole) {
            throw std::overflow_error("Whole points overflow when scaling to e4");
        }

        int64_t scaled_whole = whole_points * 10'000;
        int64_t total_points_e4 = 0;
        if (whole_points >= 0) {
            if (scaled_whole > std::numeric_limits<int64_t>::max() - static_cast<int64_t>(fractional_points_e4)) {
                throw std::overflow_error("Total points e4 addition overflow");
            }
            total_points_e4 = scaled_whole + static_cast<int64_t>(fractional_points_e4);
        } else {
            // Negative whole points
            if (scaled_whole < std::numeric_limits<int64_t>::min() + static_cast<int64_t>(fractional_points_e4)) {
                throw std::underflow_error("Total points e4 addition underflow");
            }
            total_points_e4 = scaled_whole - static_cast<int64_t>(fractional_points_e4);
        }

        if (total_points_e4 % static_cast<int64_t>(tick_size_e4) != 0) {
            throw std::invalid_argument("Price is not an exact multiple of the minimum tick size");
        }
        int64_t ticks = total_points_e4 / static_cast<int64_t>(tick_size_e4);
        return PriceInTicks(ticks);
    }

    /// Create PriceInTicks from double floating-point price and tick size in double points (e.g. 0.25).
    /// Rejects NaN, Infinity, non-positive or non-finite values.
    /// Checks exact tick-grid alignment within precision epsilon and rejects unaligned / fractional ticks.
    /// Throws without silent rounding or clamping.
    [[nodiscard]] static PriceInTicks from_double(double price_points, double tick_size) {
        if (!std::isfinite(price_points)) {
            throw std::invalid_argument("Floating-point price must be finite (not NaN or Inf)");
        }
        if (!std::isfinite(tick_size) || tick_size <= 0.0) {
            throw std::invalid_argument("Tick size must be finite and strictly positive");
        }

        double ticks_d = price_points / tick_size;
        // Check overflow for int64_t conversion range
        if (ticks_d > static_cast<double>(std::numeric_limits<int64_t>::max()) ||
            ticks_d < static_cast<double>(std::numeric_limits<int64_t>::min())) {
            throw std::overflow_error("Floating-point price ticks exceed int64_t range");
        }

        double nearest = std::round(ticks_d);
        constexpr double tolerance = 1e-7;
        if (std::abs(ticks_d - nearest) > tolerance) {
            throw std::invalid_argument("Floating-point price does not align exactly to tick grid");
        }

        return PriceInTicks(static_cast<int64_t>(nearest));
    }

    [[nodiscard]] constexpr int64_t ticks() const noexcept { return ticks_; }

    [[nodiscard]] constexpr bool operator==(const PriceInTicks& other) const noexcept { return ticks_ == other.ticks_; }
    [[nodiscard]] constexpr bool operator!=(const PriceInTicks& other) const noexcept { return ticks_ != other.ticks_; }
    [[nodiscard]] constexpr bool operator<(const PriceInTicks& other) const noexcept { return ticks_ < other.ticks_; }
    [[nodiscard]] constexpr bool operator<=(const PriceInTicks& other) const noexcept { return ticks_ <= other.ticks_; }
    [[nodiscard]] constexpr bool operator>(const PriceInTicks& other) const noexcept { return ticks_ > other.ticks_; }
    [[nodiscard]] constexpr bool operator>=(const PriceInTicks& other) const noexcept { return ticks_ >= other.ticks_; }

    [[nodiscard]] constexpr PriceInTicks operator+(const PriceInTicks& other) const {
        if ((other.ticks_ > 0 && ticks_ > std::numeric_limits<int64_t>::max() - other.ticks_) ||
            (other.ticks_ < 0 && ticks_ < std::numeric_limits<int64_t>::min() - other.ticks_)) {
            throw std::overflow_error("PriceInTicks addition overflow");
        }
        return PriceInTicks(ticks_ + other.ticks_);
    }

    [[nodiscard]] constexpr PriceInTicks operator-(const PriceInTicks& other) const {
        if ((other.ticks_ < 0 && ticks_ > std::numeric_limits<int64_t>::max() + other.ticks_) ||
            (other.ticks_ > 0 && ticks_ < std::numeric_limits<int64_t>::min() + other.ticks_)) {
            throw std::underflow_error("PriceInTicks subtraction underflow");
        }
        return PriceInTicks(ticks_ - other.ticks_);
    }

    constexpr PriceInTicks& operator+=(const PriceInTicks& other) {
        *this = *this + other;
        return *this;
    }

    constexpr PriceInTicks& operator-=(const PriceInTicks& other) {
        *this = *this - other;
        return *this;
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
