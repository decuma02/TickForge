#pragma once

#include "tickforge/domain/order.hpp"
#include "tickforge/domain/price_in_ticks.hpp"
#include "tickforge/domain/quantity.hpp"
#include <list>
#include <cstddef>
#include <stdexcept>

namespace tickforge::order_book {

/// PriceLevel represents an aggregation of all resting orders at a single price.
///
/// Ownership & Invariants:
/// - Owns its resting orders via std::list<domain::Order>.
/// - Orders are strictly stored in FIFO arrival order.
/// - Maintains aggregate total_quantity() equal to the sum of remaining_quantity() of all resting orders.
/// - Provides O(1) appending to tail and O(1) removal of any order given a valid list iterator.
class PriceLevel {
public:
    explicit PriceLevel(domain::PriceInTicks price)
        : price_(price)
        , total_quantity_(domain::Quantity::zero())
    {
        if (price_.ticks() <= 0) {
            throw std::invalid_argument("PriceLevel price must be strictly positive (> 0)");
        }
    }

    [[nodiscard]] constexpr domain::PriceInTicks price() const noexcept {
        return price_;
    }

    [[nodiscard]] constexpr domain::Quantity total_quantity() const noexcept {
        return total_quantity_;
    }

    [[nodiscard]] std::size_t order_count() const noexcept {
        return orders_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return orders_.empty();
    }

    /// Appends an order to the tail of the FIFO queue.
    /// Strong exception guarantee: verifies price match and aggregate overflow before mutation.
    std::list<domain::Order>::iterator append(const domain::Order& order) {
        validate_incoming_order(order);
        domain::Quantity new_total = total_quantity_ + order.remaining_quantity();

        orders_.push_back(order);
        total_quantity_ = new_total;
        return std::prev(orders_.end());
    }

    /// Appends an order via move to the tail of the FIFO queue.
    /// Strong exception guarantee: verifies price match and aggregate overflow before mutation.
    std::list<domain::Order>::iterator append(domain::Order&& order) {
        validate_incoming_order(order);
        domain::Quantity new_total = total_quantity_ + order.remaining_quantity();

        orders_.push_back(std::move(order));
        total_quantity_ = new_total;
        return std::prev(orders_.end());
    }

    /// Erases an order using its iterator.
    /// Precondition: it must be a dereferenceable iterator belonging to this PriceLevel's orders_ list.
    /// Returns the iterator following the removed element.
    std::list<domain::Order>::iterator erase(std::list<domain::Order>::iterator it) {
        if (it == orders_.end()) {
            throw std::invalid_argument("Cannot erase end iterator in PriceLevel");
        }
        total_quantity_ = total_quantity_ - it->remaining_quantity();
        return orders_.erase(it);
    }

    /// Update aggregate quantity if a resting order's remaining quantity changes.
    /// old_remaining is the quantity before the change, new_remaining is the quantity after.
    /// Strong exception safety: validates before applying change.
    void update_order_remaining(domain::Quantity old_remaining, domain::Quantity new_remaining) {
        if (new_remaining > old_remaining) {
            domain::Quantity diff = new_remaining - old_remaining;
            total_quantity_ = total_quantity_ + diff;
        } else if (new_remaining < old_remaining) {
            domain::Quantity diff = old_remaining - new_remaining;
            total_quantity_ = total_quantity_ - diff;
        }
    }

    [[nodiscard]] const domain::Order& front() const {
        if (orders_.empty()) {
            throw std::logic_error("PriceLevel is empty; no front order");
        }
        return orders_.front();
    }

    [[nodiscard]] domain::Order& front() {
        if (orders_.empty()) {
            throw std::logic_error("PriceLevel is empty; no front order");
        }
        return orders_.front();
    }

    [[nodiscard]] const domain::Order& back() const {
        if (orders_.empty()) {
            throw std::logic_error("PriceLevel is empty; no back order");
        }
        return orders_.back();
    }

    [[nodiscard]] domain::Order& back() {
        if (orders_.empty()) {
            throw std::logic_error("PriceLevel is empty; no back order");
        }
        return orders_.back();
    }

    // Iterators for inspection and traversal
    [[nodiscard]] auto begin() const noexcept { return orders_.cbegin(); }
    [[nodiscard]] auto end() const noexcept { return orders_.cend(); }
    [[nodiscard]] auto cbegin() const noexcept { return orders_.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return orders_.cend(); }

    [[nodiscard]] auto begin() noexcept { return orders_.begin(); }
    [[nodiscard]] auto end() noexcept { return orders_.end(); }

    /// Verifies that total_quantity_ strictly matches the sum of resting orders.
    [[nodiscard]] bool check_invariants() const noexcept {
        uint64_t sum = 0;
        for (const auto& ord : orders_) {
            sum += static_cast<uint64_t>(ord.remaining_quantity().lots());
        }
        return sum == static_cast<uint64_t>(total_quantity_.lots());
    }

private:
    void validate_incoming_order(const domain::Order& order) const {
        if (order.price() != price_) {
            throw std::invalid_argument("Order price does not match PriceLevel price");
        }
        if (!order.is_active()) {
            throw std::invalid_argument("Cannot append inactive order to PriceLevel");
        }
    }

    domain::PriceInTicks price_;
    domain::Quantity total_quantity_;
    std::list<domain::Order> orders_;
};

} // namespace tickforge::order_book
