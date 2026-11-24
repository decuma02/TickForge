#pragma once

#include "price_in_ticks.hpp"
#include "quantity.hpp"
#include <string>
#include <string_view>
#include <stdexcept>
#include <cstdint>

namespace tickforge::domain {

/// Product specifications for an electronic futures product (e.g. CME Globex E-mini S&P 500).
/// Distinct from individual dated contract instances (e.g. ESM4, ESU4).
class ProductSpecification {
public:
    ProductSpecification(
        std::string commodity_code,
        std::string name,
        uint32_t tick_size_e4,         // Tick size scaled by 10^4 (e.g., 2500 for 0.25)
        uint32_t contract_multiplier,  // Dollar multiplier per full index point (e.g., 50 for $50/point)
        Quantity min_trade_vol,        // Minimum order size (CME Tag 562 = 1)
        Quantity max_trade_vol         // Maximum order size (CME Tag 1140 = 2,999 for ES outrights)
    )
        : commodity_code_(std::move(commodity_code))
        , name_(std::move(name))
        , tick_size_e4_(tick_size_e4)
        , contract_multiplier_(contract_multiplier)
        , min_trade_vol_(min_trade_vol)
        , max_trade_vol_(max_trade_vol)
    {
        if (commodity_code_.empty()) {
            throw std::invalid_argument("Product commodity code cannot be empty");
        }
        if (tick_size_e4_ == 0) {
            throw std::invalid_argument("Product tick size cannot be zero");
        }
        if (contract_multiplier_ == 0) {
            throw std::invalid_argument("Product contract multiplier cannot be zero");
        }
        if (min_trade_vol_.is_zero()) {
            throw std::invalid_argument("Min trade volume cannot be zero");
        }
        if (max_trade_vol_ < min_trade_vol_) {
            throw std::invalid_argument("Max trade volume cannot be less than min trade volume");
        }
    }

    /// Factory for CME Globex E-mini S&P 500 (ES) Outright Futures specification.
    /// - Commodity Code: "ES"
    /// - Name: "E-mini S&P 500 Index Futures"
    /// - Tick size: 0.25 index points (2500 with e4 scale)
    /// - Contract multiplier: $50 per index point ($12.50 per tick)
    /// - Min trade volume: 1 contract (Tag 562)
    /// - Max trade volume: 2,999 contracts (Tag 1140 baseline)
    [[nodiscard]] static ProductSpecification cme_es_outright() {
        return ProductSpecification(
            "ES",
            "E-mini S&P 500 Index Futures",
            2'500,
            50,
            Quantity(1),
            Quantity(2'999)
        );
    }

    [[nodiscard]] const std::string& commodity_code() const noexcept { return commodity_code_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] constexpr uint32_t tick_size_e4() const noexcept { return tick_size_e4_; }
    [[nodiscard]] constexpr uint32_t contract_multiplier() const noexcept { return contract_multiplier_; }
    [[nodiscard]] constexpr Quantity min_trade_vol() const noexcept { return min_trade_vol_; }
    [[nodiscard]] constexpr Quantity max_trade_vol() const noexcept { return max_trade_vol_; }

    /// Validate whether a requested order quantity conforms to product trade volume bounds.
    [[nodiscard]] bool validate_quantity(Quantity qty) const noexcept {
        return qty >= min_trade_vol_ && qty <= max_trade_vol_;
    }

    /// Convert a decimal index price into PriceInTicks using this product's tick size.
    [[nodiscard]] PriceInTicks to_price_in_ticks(int64_t whole_points, uint32_t fractional_points_e4) const {
        return PriceInTicks::from_index_points(whole_points, fractional_points_e4, tick_size_e4_);
    }

private:
    std::string commodity_code_;
    std::string name_;
    uint32_t tick_size_e4_;
    uint32_t contract_multiplier_;
    Quantity min_trade_vol_;
    Quantity max_trade_vol_;
};

} // namespace tickforge::domain
