#pragma once

#include <cstdint>
#include <string>
#include <limits>
#include <stdexcept>
#include <ostream>

namespace tickforge::domain {

/// Strongly-typed discrete contract quantity in whole lots.
/// Uses uint32_t per project specification.
/// Explicit validation: zero quantities are invalid; overflow is detected.
class Quantity {
public:
    constexpr Quantity() noexcept : lots_(0) {}

    /// Construct Quantity from whole lots. Validates lots > 0.
    /// Does not silently clamp or saturate.
    constexpr explicit Quantity(uint32_t lots) : lots_(lots) {
        if (lots == 0) {
            throw std::invalid_argument("Quantity must be strictly positive (> 0)");
        }
    }

    /// Construct sentinel or zero Quantity for internal arithmetic tracking.
    [[nodiscard]] static constexpr Quantity zero() noexcept {
        Quantity q;
        q.lots_ = 0;
        return q;
    }

    /// Construct Quantity allowing zero lots (e.g. when order is completely executed).
    [[nodiscard]] static constexpr Quantity from_raw(uint32_t lots) noexcept {
        Quantity q;
        q.lots_ = lots;
        return q;
    }

    [[nodiscard]] constexpr uint32_t lots() const noexcept { return lots_; }
    [[nodiscard]] constexpr bool is_zero() const noexcept { return lots_ == 0; }

    [[nodiscard]] constexpr bool operator==(const Quantity& other) const noexcept { return lots_ == other.lots_; }
    [[nodiscard]] constexpr bool operator!=(const Quantity& other) const noexcept { return lots_ != other.lots_; }
    [[nodiscard]] constexpr bool operator<(const Quantity& other) const noexcept { return lots_ < other.lots_; }
    [[nodiscard]] constexpr bool operator<=(const Quantity& other) const noexcept { return lots_ <= other.lots_; }
    [[nodiscard]] constexpr bool operator>(const Quantity& other) const noexcept { return lots_ > other.lots_; }
    [[nodiscard]] constexpr bool operator>=(const Quantity& other) const noexcept { return lots_ >= other.lots_; }

    [[nodiscard]] constexpr Quantity operator+(const Quantity& other) const {
        if (other.lots_ > std::numeric_limits<uint32_t>::max() - lots_) {
            throw std::overflow_error("Quantity addition overflow");
        }
        return from_raw(lots_ + other.lots_);
    }

    [[nodiscard]] constexpr Quantity operator-(const Quantity& other) const {
        if (lots_ < other.lots_) {
            throw std::underflow_error("Quantity subtraction underflow (result would be negative)");
        }
        return from_raw(lots_ - other.lots_);
    }

    constexpr Quantity& operator+=(const Quantity& other) {
        *this = *this + other;
        return *this;
    }

    constexpr Quantity& operator-=(const Quantity& other) {
        *this = *this - other;
        return *this;
    }

    [[nodiscard]] std::string to_string() const {
        return std::to_string(lots_);
    }

private:
    uint32_t lots_;
};

inline std::ostream& operator<<(std::ostream& os, const Quantity& qty) {
    return os << qty.lots();
}

} // namespace tickforge::domain

namespace std {
template <>
struct hash<tickforge::domain::Quantity> {
    size_t operator()(const tickforge::domain::Quantity& q) const noexcept {
        return hash<uint32_t>{}(q.lots());
    }
};
} // namespace std
