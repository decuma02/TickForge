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
    /// Determine if a year is a leap year according to Gregorian calendar rules.
    [[nodiscard]] static constexpr bool is_leap_year(uint32_t year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    /// Return the number of days in a given month of a given year (1-12).
    /// Returns 0 for invalid months.
    [[nodiscard]] static constexpr uint32_t days_in_month(uint32_t year, uint32_t month) noexcept {
        switch (month) {
            case 1:  return 31;
            case 2:  return is_leap_year(year) ? 29 : 28;
            case 3:  return 31;
            case 4:  return 30;
            case 5:  return 31;
            case 6:  return 30;
            case 7:  return 31;
            case 8:  return 31;
            case 9:  return 30;
            case 10: return 31;
            case 11: return 30;
            case 12: return 31;
            default: return 0;
        }
    }

    /// Validates whether a YYYYMMDD integer represents a valid calendar date
    /// between years 1970 and 2100 inclusive.
    [[nodiscard]] static constexpr bool is_valid_calendar_date(uint32_t yyyymmdd) noexcept {
        uint32_t y = yyyymmdd / 10'000;
        uint32_t m = (yyyymmdd / 100) % 100;
        uint32_t d = yyyymmdd % 100;

        if (y < 1970 || y > 2100 || m < 1 || m > 12) {
            return false;
        }
        uint32_t max_days = days_in_month(y, m);
        return d >= 1 && d <= max_days;
    }

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

        if (!is_valid_calendar_date(expiration_date_yyyymmdd_)) {
            throw std::invalid_argument("Invalid expiration date YYYYMMDD format or calendar day");
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

    /// Check if the contract is expired relative to an explicitly supplied reference date (YYYYMMDD).
    /// Pure deterministic function independent of host system wall-clock time.
    [[nodiscard]] constexpr bool is_expired_at(uint32_t reference_date_yyyymmdd) const noexcept {
        return reference_date_yyyymmdd > expiration_date_yyyymmdd_;
    }

    /// Explicitly transition trading status to Expired if reference date exceeds expiration date.
    /// Preserves full historical determinism for simulation replays.
    void update_expiration_status(uint32_t reference_date_yyyymmdd) noexcept {
        if (is_expired_at(reference_date_yyyymmdd)) {
            status_ = ContractTradingStatus::Expired;
        }
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
