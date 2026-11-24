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
///   initial_qty == executed_qty + remaining_qty + reduced_qty + cancelled_qty
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
        , initial_quantity_(initial_quantity)
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
        if (initial_quantity_.is_zero()) {
            throw std::invalid_argument("Initial quantity must be strictly positive (> 0)");
        }
    }

    [[nodiscard]] constexpr OrderId id() const noexcept { return id_; }
    [[nodiscard]] constexpr Side side() const noexcept { return side_; }
    [[nodiscard]] constexpr PriceInTicks price() const noexcept { return price_; }
    [[nodiscard]] constexpr Quantity initial_quantity() const noexcept { return initial_quantity_; }
    [[nodiscard]] constexpr Quantity remaining_quantity() const noexcept { return remaining_quantity_; }
    [[nodiscard]] constexpr Quantity executed_quantity() const noexcept { return executed_quantity_; }
    [[nodiscard]] constexpr Quantity reduced_quantity() const noexcept { return reduced_quantity_; }
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
    /// initial_quantity == executed_quantity + remaining_quantity + reduced_quantity + cancelled_quantity
    [[nodiscard]] constexpr bool check_quantity_invariant() const noexcept {
        uint64_t sum = static_cast<uint64_t>(executed_quantity_.lots())
                     + static_cast<uint64_t>(remaining_quantity_.lots())
                     + static_cast<uint64_t>(reduced_quantity_.lots())
                     + static_cast<uint64_t>(cancelled_quantity_.lots());
        return sum == static_cast<uint64_t>(initial_quantity_.lots());
    }

    /// Record an execution fill.
    /// Decreases remaining_quantity and increases executed_quantity by fill_qty.
    void record_execution(Quantity fill_qty) {
        if (fill_qty.is_zero()) {
            throw std::invalid_argument("Fill quantity must be greater than zero");
        }
        if (fill_qty > remaining_quantity_) {
            throw std::invalid_argument("Fill quantity exceeds remaining working quantity");
        }

        remaining_quantity_ -= fill_qty;
        executed_quantity_ += fill_qty;

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
    void record_quantity_reduction(Quantity new_working_qty) {
        if (new_working_qty.is_zero()) {
            throw std::invalid_argument("New working quantity cannot be zero; use cancellation instead");
        }
        if (new_working_qty >= remaining_quantity_) {
            throw std::invalid_argument("record_quantity_reduction requires new_working_qty < remaining_quantity");
        }

        Quantity reduction = remaining_quantity_ - new_working_qty;
        remaining_quantity_ = new_working_qty;
        reduced_quantity_ += reduction;

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after reduction");
        }
    }

    /// Record a quantity increase via ModifyOrder.
    /// Increases remaining_quantity and updates initial_quantity to preserve invariant,
    /// and assigns a new engine priority sequence number (loss of priority).
    void record_quantity_increase(Quantity new_working_qty, uint64_t new_priority_seq) {
        if (new_working_qty <= remaining_quantity_) {
            throw std::invalid_argument("record_quantity_increase requires new_working_qty > remaining_quantity");
        }

        Quantity increase = new_working_qty - remaining_quantity_;
        remaining_quantity_ = new_working_qty;
        initial_quantity_ += increase;
        priority_sequence_ = new_priority_seq;

        if (!check_quantity_invariant()) {
            throw std::logic_error("Order quantity accounting invariant violated after increase");
        }
    }

    /// Record a price change via ModifyOrder.
    /// Updates price and assigns a new engine priority sequence number (loss of priority).
    void update_price(PriceInTicks new_price, uint64_t new_priority_seq) {
        if (new_price.ticks() <= 0) {
            throw std::invalid_argument("New price must be strictly positive (> 0)");
        }
        price_ = new_price;
        priority_sequence_ = new_priority_seq;
    }

    /// Cancel any remaining working quantity.
    /// Sets remaining_quantity to 0 and tracks cancelled_quantity.
    void cancel() {
        if (!is_active()) {
            throw std::invalid_argument("Cannot cancel inactive order");
        }
        cancelled_quantity_ += remaining_quantity_;
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
    Quantity initial_quantity_;
    Quantity remaining_quantity_;
    Quantity executed_quantity_;
    Quantity reduced_quantity_;
    Quantity cancelled_quantity_;
    uint64_t priority_sequence_;
    OrderStatus status_;
};

} // namespace tickforge::domain
