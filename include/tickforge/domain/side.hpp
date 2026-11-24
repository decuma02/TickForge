#pragma once

#include <cstdint>
#include <string_view>
#include <ostream>
#include <stdexcept>

namespace tickforge::domain {

/// Order book side: Buy (Bid) or Sell (Ask).
enum class Side : uint8_t {
    Buy = 1,
    Sell = 2
};

[[nodiscard]] constexpr Side opposite_side(Side side) {
    switch (side) {
        case Side::Buy:  return Side::Sell;
        case Side::Sell: return Side::Buy;
    }
    throw std::invalid_argument("Invalid Side value");
}

[[nodiscard]] constexpr std::string_view to_string(Side side) noexcept {
    switch (side) {
        case Side::Buy:  return "BUY";
        case Side::Sell: return "SELL";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, Side side) {
    return os << to_string(side);
}

} // namespace tickforge::domain
