#pragma once

#include "product_specification.hpp"
#include <string>
#include <string_view>
#include <stdexcept>
#include <cstdint>

namespace tickforge::domain {

/// Trading status of a contract instance.
enum class ContractTradingStatus : uint8_t {
    PreOpen = 1,
    ContinuousTrading = 2,
    Halted = 3,
    Expired = 4
};

[[nodiscard]] constexpr std::string_view to_string(ContractTradingStatus status) noexcept {
    switch (status) {
        case ContractTradingStatus::PreOpen:           return "PRE_OPEN";
        case ContractTradingStatus::ContinuousTrading: return "CONTINUOUS_TRADING";
        case ContractTradingStatus::Halted:            return "HALTED";
        case ContractTradingStatus::Expired:           return "EXPIRED";
    }
    return "UNKNOWN";
}

/// An individual tradeable contract instance of a product (e.g. "ESM4", representing the June 2024 ES future).
/// Contains contract instance identification, maturity year/month, expiration metadata, and trading status.
/// Contract expiry transitions and settlement are deferred, but the metadata belongs in the domain model.
class ContractInstance {
public:
    ContractInstance(
        std::string symbol,
        const ProductSpecification& product_spec,
        uint16_t maturity_year,
        uint8_t maturity_month,
        uint32_t expiration_date_yyyymmdd
    )
        : symbol_(std::move(symbol))
        , commodity_code_(product_spec.commodity_code())
        , maturity_year_(maturity_year)
        , maturity_month_(maturity_month)
        , expiration_date_yyyymmdd_(expiration_date_yyyymmdd)
        , status_(ContractTradingStatus::ContinuousTrading)
    {
        if (symbol_.empty()) {
            throw std::invalid_argument("Contract symbol cannot be empty");
        }
        if (maturity_month_ < 1 || maturity_month_ > 12) {
            throw std::invalid_argument("Maturity month must be between 1 and 12");
        }
        if (maturity_year_ < 1970 || maturity_year_ > 2100) {
            throw std::invalid_argument("Maturity year outside valid range [1970, 2100]");
        }
        if (expiration_date_yyyymmdd_ < 19700101) {
            throw std::invalid_argument("Invalid expiration date YYYYMMDD format");
        }
    }

    [[nodiscard]] const std::string& symbol() const noexcept { return symbol_; }
    [[nodiscard]] const std::string& commodity_code() const noexcept { return commodity_code_; }
    [[nodiscard]] constexpr uint16_t maturity_year() const noexcept { return maturity_year_; }
    [[nodiscard]] constexpr uint8_t maturity_month() const noexcept { return maturity_month_; }
    [[nodiscard]] constexpr uint32_t expiration_date() const noexcept { return expiration_date_yyyymmdd_; }
    [[nodiscard]] constexpr ContractTradingStatus status() const noexcept { return status_; }

    void set_status(ContractTradingStatus status) noexcept {
        status_ = status;
    }

    [[nodiscard]] constexpr bool is_tradable() const noexcept {
        return status_ == ContractTradingStatus::ContinuousTrading;
    }

    [[nodiscard]] bool operator==(const ContractInstance& other) const noexcept {
        return symbol_ == other.symbol_;
    }

    [[nodiscard]] bool operator!=(const ContractInstance& other) const noexcept {
        return !(*this == other);
    }

private:
    std::string symbol_;
    std::string commodity_code_;
    uint16_t maturity_year_;
    uint8_t maturity_month_;
    uint32_t expiration_date_yyyymmdd_;
    ContractTradingStatus status_;
};

} // namespace tickforge::domain
