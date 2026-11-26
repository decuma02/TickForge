#pragma once

#include "order_id.hpp"
#include "side.hpp"
#include "price_in_ticks.hpp"
#include "quantity.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>

namespace tickforge::domain {

/// Lifecycle status of an Order.
enum class OrderStatus : uint8_t {
    New = 1,
    PartiallyFilled = 2,
    Filled = 3,
    Cancelled = 4
};

[[nodiscard]] constexpr std::string_view to_string(OrderStatus status) noexcept {
    switch (status) {
        case OrderStatus::New:             return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled:          return "FILLED";
        case OrderStatus::Cancelled:       return "CANCELLED";
    }
    return "UNKNOWN";
}

/// Order represents a stateful limit order in the matching engine.
///
/// Order Quantity Accounting Invariant:
///   total_submitted_quantity == executed_quantity + remaining_quantity + reduced_quantity + cancelled_quantity
/// where:
///   total_submitted_quantity == original_submitted_quantity + cumulative_added_quantity
///
/// This preserves strict distinction between original submitted quantity and modifications,
/// without silently mutating original_submitted_quantity.
///
/// Priority is tracked by priority_seq (monotonically increasing engine sequence),
/// not by wall-clock time.
class Order {
public:
    Order(
        OrderId id,
        Side side,
        PriceInTicks price,
        Quantity initial_quantity,
        uint64_t priority_sequence
    )
        : id_(id)
        , side_(side)
        , price_(price)
        , original_submitted_quantity_(initial_quantity)
        , cumulative_added_quantity_(Quantity::zero())
        , remaining_quantity_(initial_quantity)
        , executed_quantity_(Quantity::zero())
        , reduced_quantity_(Quantity::zero())
        , cancelled_quantity_(Quantity::zero())
        , priority_sequence_(priority_sequence)
        , status_(OrderStatus::New)
    {
        if (!id_.is_valid()) {
            throw std::invalid_argument("OrderId must be non-zero");
        }
        if (price_.ticks() <= 0) {
            throw std::invalid_argument("Order price must be strictly positive (> 0)");
        }
        if (initial_quantity.is_zero()) {
            throw std::invalid_argument("Initial quantity must be strictly positive (> 0)");
        }
    }

    [[nodiscard]] constexpr OrderId id() const noexcept { return id_; }
    [[nodiscard]] constexpr Side side() const noexcept { return side_; }
    [[nodiscard]] constexpr PriceInTicks price() const noexcept { return price_; }

    /// Original submitted quantity at order placement (immutable across modifications).
    [[nodiscard]] constexpr Quantity original_submitted_quantity() const noexcept { return original_submitted_quantity_; }

    /// Backward compatibility alias for original_submitted_quantity.
    [[nodiscard]] constexpr Quantity initial_quantity() const noexcept { return original_submitted_quantity_; }

    /// Cumulative quantity added by upward modifications.
    [[nodiscard]] constexpr Quantity cumulative_added_quantity() const noexcept { return cumulative_added_quantity_; }

    /// Total submitted quantity: original_submitted_quantity + cumulative_added_quantity.
    [[nodiscard]] constexpr Quantity total_submitted_quantity() const {
        return original_submitted_quantity_ + cumulative_added_quantity_;
    }

    /// Currently active/working quantity resting in the book.
    [[nodiscard]] constexpr Quantity remaining_quantity() const noexcept { return remaining_quantity_; }

    /// Cumulative executed quantity filled across trades.
    [[nodiscard]] constexpr Quantity executed_quantity() const noexcept { return executed_quantity_; }

    /// Cumulative quantity removed by downward modifications.
    [[nodiscard]] constexpr Quantity reduced_quantity() const noexcept { return reduced_quantity_; }

    /// Cumulative quantity cancelled.
    [[nodiscard]] constexpr Quantity cancelled_quantity() const noexcept { return cancelled_quantity_; }

    [[nodiscard]] constexpr uint64_t priority_sequence() const noexcept { return priority_sequence_; }
    [[nodiscard]] constexpr OrderStatus status() const noexcept { return status_; }

    [[nodiscard]] constexpr bool is_active() const noexcept {
        return status_ == OrderStatus::New || status_ == OrderStatus::PartiallyFilled;
    }

    [[nodiscard]] constexpr bool is_filled() const noexcept {
        return status_ == OrderStatus::Filled;
    }

    [[nodiscard]] constexpr bool is_cancelled() const noexcept {
        return status_ == OrderStatus::Cancelled;
    }

    /// Verifies the invariant:
    /// total_submitted_quantity == executed_quantity + remaining_quantity + reduced_quantity + cancelled_quantity
    [[nodiscard]] constexpr bool check_quantity_invariant() const noexcept {
        uint64_t left = static_cast<uint64_t>(original_submitted_quantity_.lots())
                      + static_cast<uint64_t>(cumulative_added_quantity_.lots());

        uint64_t right = static_cast<uint64_t>(executed_quantity_.lots())
                       + static_cast<uint64_t>(remaining_quantity_.lots())
                       + static_cast<uint64_t>(reduced_quantity_.lots())
                       + static_cast<uint64_t>(cancelled_quantity_.lots());
        return left == right;
    }

    /// Record an execution fill.
    /// Decreases remaining_quantity and increases executed_quantity by fill_qty.
    /// Strong exception guarantee: fails cleanly before mutating any state if validation fails.
    void record_execution(Quantity fill_qty) {
        if (!is_active()) {
            throw std::invalid_argument("Cannot execute against inactive order");
        }
        if (fill_qty.is_zero()) {
            throw std::invalid_argument("Fill quantity must be greater than zero");
        }
        if (fill_qty > remaining_quantity_) {
            throw std::invalid_argument("Fill quantity exceeds remaining working quantity");
        }

        // Test addition for overflow safety before mutating state
        Quantity new_executed = executed_quantity_ + fill_qty;
        Quantity new_remaining = remaining_quantity_ - fill_qty;

        remaining_quantity_ = new_remaining;
        executed_quantity_ = new_executed;

        if (remaining_quantity_.is_zero()) {
            status_ = OrderStatus::Filled;
        } else {
            status_ = OrderStatus::PartiallyFilled;
        }

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after execution");
        }
    }

    /// Record a quantity reduction via ModifyOrder.
    /// Reduces remaining_quantity and tracks the difference as reduced_quantity.
    /// Retains existing priority_sequence.
    /// Strong exception guarantee: validates and tests all additions before mutation.
    void record_quantity_reduction(Quantity new_working_qty) {
        if (!is_active()) {
            throw std::invalid_argument("Cannot modify inactive order");
        }
        if (new_working_qty.is_zero()) {
            throw std::invalid_argument("New working quantity cannot be zero; use cancellation instead");
        }
        if (new_working_qty >= remaining_quantity_) {
            throw std::invalid_argument("record_quantity_reduction requires new_working_qty < remaining_quantity");
        }

        Quantity reduction = remaining_quantity_ - new_working_qty;
        // Test addition for overflow safety before mutating state
        Quantity new_reduced = reduced_quantity_ + reduction;

        remaining_quantity_ = new_working_qty;
        reduced_quantity_ = new_reduced;

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after reduction");
        }
    }

    /// Record a quantity increase via ModifyOrder.
    /// Increases remaining_quantity and cumulative_added_quantity (leaving original_submitted_quantity untouched),
    /// and assigns a new engine priority sequence number (loss of priority).
    /// Strong exception guarantee: verifies overflow safety on cumulative additions before mutating state.
    void record_quantity_increase(Quantity new_working_qty, uint64_t new_priority_seq) {
        if (!is_active()) {
            throw std::invalid_argument("Cannot modify inactive order");
        }
        if (new_working_qty <= remaining_quantity_) {
            throw std::invalid_argument("record_quantity_increase requires new_working_qty > remaining_quantity");
        }

        Quantity increase = new_working_qty - remaining_quantity_;
        // Test cumulative added addition for overflow safety before mutating state
        Quantity new_added = cumulative_added_quantity_ + increase;

        remaining_quantity_ = new_working_qty;
        cumulative_added_quantity_ = new_added;
        priority_sequence_ = new_priority_seq;

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after increase");
        }
    }

    /// Record a price change via ModifyOrder.
    /// Updates price and assigns a new engine priority sequence number (loss of priority).
    /// Strong exception guarantee: validates inputs before mutating state.
    void update_price(PriceInTicks new_price, uint64_t new_priority_seq) {
        if (!is_active()) {
            throw std::invalid_argument("Cannot modify price of inactive order");
        }
        if (new_price.ticks() <= 0) {
            throw std::invalid_argument("New price must be strictly positive (> 0)");
        }
        price_ = new_price;
        priority_sequence_ = new_priority_seq;
    }

    /// Cancel any remaining working quantity.
    /// Sets remaining_quantity to 0 and tracks cancelled_quantity.
    /// Strong exception guarantee: tests addition before mutating state.
    void cancel() {
        if (!is_active()) {
            throw std::invalid_argument("Cannot cancel inactive order");
        }
        // Test addition for overflow safety before mutating state
        Quantity new_cancelled = cancelled_quantity_ + remaining_quantity_;

        cancelled_quantity_ = new_cancelled;
        remaining_quantity_ = Quantity::zero();
        status_ = OrderStatus::Cancelled;

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after cancellation");
        }
    }

private:
    OrderId id_;
    Side side_;
    PriceInTicks price_;
    Quantity original_submitted_quantity_;
    Quantity cumulative_added_quantity_;
    Quantity remaining_quantity_;
    Quantity executed_quantity_;
    Quantity reduced_quantity_;
    Quantity cancelled_quantity_;
    uint64_t priority_sequence_;
    OrderStatus status_;
};

} // namespace tickforge::domain
