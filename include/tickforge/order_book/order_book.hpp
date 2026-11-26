#pragma once

#include "price_level.hpp"
#include "order_locator.hpp"
#include "tickforge/domain/contract_instance.hpp"
#include "tickforge/domain/order.hpp"
#include "tickforge/domain/order_id.hpp"
#include "tickforge/domain/price_in_ticks.hpp"
#include "tickforge/domain/quantity.hpp"
#include "tickforge/domain/side.hpp"

#include <map>
#include <unordered_map>
#include <functional>
#include <optional>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace tickforge::order_book {

/// OrderBook maintains the resting limit orders for a single ContractInstance.
///
/// Architecture & Decisions (ADR-0002, ADR-0003):
/// - Bid-side price levels are organized in descending order (std::greater<PriceInTicks>),
///   so best bid is immediately accessible at bids_.begin().
/// - Ask-side price levels are organized in ascending order (std::less<PriceInTicks>),
///   so best ask is immediately accessible at asks_.begin().
/// - Fast O(1) order lookup and removal is enabled by locator_index_:
///   std::unordered_map<domain::OrderId, OrderLocator>.
/// - Non-copyable and non-movable to guarantee iterator validity and prevent dangling locators.
/// - Invariants:
///   1. Every resting order in any price level has exactly one entry in locator_index_.
///   2. Every entry in locator_index_ points to a valid live order with matching ID, side, and price.
///   3. Empty price levels are removed immediately upon removal/execution of their final order.
///   4. Bids and asks remain independent.
///   5. No duplicate order IDs permitted.
class OrderBook {
public:
    using BidMap = std::map<domain::PriceInTicks, PriceLevel, std::greater<domain::PriceInTicks>>;
    using AskMap = std::map<domain::PriceInTicks, PriceLevel, std::less<domain::PriceInTicks>>;
    using LocatorIndex = std::unordered_map<domain::OrderId, OrderLocator>;

    explicit OrderBook(domain::ContractInstance contract)
        : contract_(contract)
    {}

    // Non-copyable and non-movable to preserve absolute iterator stability for locators.
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;
    ~OrderBook() = default;

    [[nodiscard]] const domain::ContractInstance& contract() const noexcept {
        return contract_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return bids_.empty() && asks_.empty();
    }

    [[nodiscard]] std::size_t order_count() const noexcept {
        return locator_index_.size();
    }

    [[nodiscard]] std::size_t level_count(domain::Side side) const noexcept {
        return (side == domain::Side::Buy) ? bids_.size() : asks_.size();
    }

    [[nodiscard]] std::size_t total_level_count() const noexcept {
        return bids_.size() + asks_.size();
    }

    /// Best bid price at the top of the bid book.
    [[nodiscard]] std::optional<domain::PriceInTicks> best_bid_price() const noexcept {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.begin()->first;
    }

    /// Best ask price at the top of the ask book.
    [[nodiscard]] std::optional<domain::PriceInTicks> best_ask_price() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }

    /// Pointer to the best bid PriceLevel, or nullptr if no bids exist.
    [[nodiscard]] const PriceLevel* best_bid_level() const noexcept {
        if (bids_.empty()) {
            return nullptr;
        }
        return &bids_.begin()->second;
    }

    /// Pointer to the best ask PriceLevel, or nullptr if no asks exist.
    [[nodiscard]] const PriceLevel* best_ask_level() const noexcept {
        if (asks_.empty()) {
            return nullptr;
        }
        return &asks_.begin()->second;
    }

    /// Finds a PriceLevel by side and price. Returns nullptr if level does not exist.
    [[nodiscard]] const PriceLevel* find_level(domain::Side side, domain::PriceInTicks price) const noexcept {
        if (side == domain::Side::Buy) {
            auto it = bids_.find(price);
            return (it != bids_.end()) ? &it->second : nullptr;
        } else {
            auto it = asks_.find(price);
            return (it != asks_.end()) ? &it->second : nullptr;
        }
    }

    /// Lookup an order by its OrderId. Returns pointer to order or nullptr if not found.
    [[nodiscard]] const domain::Order* find_order(domain::OrderId id) const noexcept {
        auto it = locator_index_.find(id);
        if (it == locator_index_.end()) {
            return nullptr;
        }
        return &(*(it->second.order_iter));
    }

    /// Add a resting limit order to the book.
    ///
    /// Preconditions:
    /// - Order must have a valid ID not already present in the book.
    /// - Order must be active.
    /// - Order price must be > 0.
    /// - Order remaining quantity must be > 0.
    ///
    /// Strong exception guarantee: If insertion fails, the book is left unchanged.
    bool insert_order(const domain::Order& order) {
        return insert_order_impl(order);
    }

    bool insert_order(domain::Order&& order) {
        return insert_order_impl(std::move(order));
    }

    /// Remove a resting order from the book by its OrderId.
    ///
    /// If the order is the last order at its price level, the empty PriceLevel is erased from the map.
    /// Returns true if removed, false if order ID was not found.
    bool remove_order(domain::OrderId id) {
        auto loc_it = locator_index_.find(id);
        if (loc_it == locator_index_.end()) {
            return false;
        }

        const OrderLocator& locator = loc_it->second;
        if (locator.side == domain::Side::Buy) {
            auto level_it = bids_.find(locator.price);
            if (level_it == bids_.end()) {
                throw std::logic_error("Internal error: bid level missing for order locator");
            }
            level_it->second.erase(locator.order_iter);
            if (level_it->second.empty()) {
                bids_.erase(level_it);
            }
        } else {
            auto level_it = asks_.find(locator.price);
            if (level_it == asks_.end()) {
                throw std::logic_error("Internal error: ask level missing for order locator");
            }
            level_it->second.erase(locator.order_iter);
            if (level_it->second.empty()) {
                asks_.erase(level_it);
            }
        }

        locator_index_.erase(loc_it);
        return true;
    }

    /// Verifies all L3 OrderBook invariants:
    /// 1. locator_index_ size equals sum of order counts across all price levels.
    /// 2. Every order in every price level has a matching entry in locator_index_.
    /// 3. Every entry in locator_index_ points to the exact order.
    /// 4. No empty price levels exist in bids_ or asks_.
    /// 5. Each price level's internal invariants (FIFO & aggregate quantity) hold.
    [[nodiscard]] bool check_invariants() const noexcept {
        std::size_t counted_orders = 0;

        // Check Bids
        domain::PriceInTicks prev_bid_price(0);
        bool first_bid = true;
        for (const auto& [price, level] : bids_) {
            if (level.empty()) {
                return false; // Invariant violated: empty level in map
            }
            if (price != level.price()) {
                return false;
            }
            if (!level.check_invariants()) {
                return false;
            }
            if (!first_bid && price >= prev_bid_price) {
                return false; // Bids must be strictly descending
            }
            prev_bid_price = price;
            first_bid = false;

            for (const auto& ord : level) {
                ++counted_orders;
                auto loc_it = locator_index_.find(ord.id());
                if (loc_it == locator_index_.end()) {
                    return false;
                }
                if (loc_it->second.side != domain::Side::Buy ||
                    loc_it->second.price != price ||
                    loc_it->second.order_iter->id() != ord.id()) {
                    return false;
                }
            }
        }

        // Check Asks
        domain::PriceInTicks prev_ask_price(0);
        bool first_ask = true;
        for (const auto& [price, level] : asks_) {
            if (level.empty()) {
                return false; // Invariant violated: empty level in map
            }
            if (price != level.price()) {
                return false;
            }
            if (!level.check_invariants()) {
                return false;
            }
            if (!first_ask && price <= prev_ask_price) {
                return false; // Asks must be strictly ascending
            }
            prev_ask_price = price;
            first_ask = false;

            for (const auto& ord : level) {
                ++counted_orders;
                auto loc_it = locator_index_.find(ord.id());
                if (loc_it == locator_index_.end()) {
                    return false;
                }
                if (loc_it->second.side != domain::Side::Sell ||
                    loc_it->second.price != price ||
                    loc_it->second.order_iter->id() != ord.id()) {
                    return false;
                }
            }
        }

        return counted_orders == locator_index_.size();
    }

    [[nodiscard]] const BidMap& bids() const noexcept { return bids_; }
    [[nodiscard]] const AskMap& asks() const noexcept { return asks_; }

private:
    template <typename OrderT>
    bool insert_order_impl(OrderT&& order) {
        if (!order.id().is_valid()) {
            throw std::invalid_argument("Cannot insert order with invalid or zero OrderId");
        }
        if (!order.is_active()) {
            throw std::invalid_argument("Cannot insert inactive order into OrderBook");
        }
        if (order.price().ticks() <= 0) {
            throw std::invalid_argument("Order price must be strictly positive (> 0)");
        }
        if (order.remaining_quantity().is_zero()) {
            throw std::invalid_argument("Order remaining quantity must be strictly positive");
        }

        domain::OrderId order_id = order.id();
        domain::Side side = order.side();
        domain::PriceInTicks price = order.price();

        // Duplicate order ID check
        if (locator_index_.find(order_id) != locator_index_.end()) {
            throw std::invalid_argument("Duplicate OrderId: order already exists in OrderBook");
        }

        // Find or create price level
        if (side == domain::Side::Buy) {
            bool level_created = false;
            auto level_it = bids_.find(price);
            if (level_it == bids_.end()) {
                auto emplace_res = bids_.emplace(price, PriceLevel(price));
                level_it = emplace_res.first;
                level_created = true;
            }

            try {
                auto list_it = level_it->second.append(std::forward<OrderT>(order));
                try {
                    locator_index_.emplace(order_id, OrderLocator{side, price, list_it});
                } catch (...) {
                    level_it->second.erase(list_it);
                    if (level_created && level_it->second.empty()) {
                        bids_.erase(level_it);
                    }
                    throw;
                }
            } catch (...) {
                if (level_created && level_it->second.empty()) {
                    bids_.erase(level_it);
                }
                throw;
            }
        } else {
            bool level_created = false;
            auto level_it = asks_.find(price);
            if (level_it == asks_.end()) {
                auto emplace_res = asks_.emplace(price, PriceLevel(price));
                level_it = emplace_res.first;
                level_created = true;
            }

            try {
                auto list_it = level_it->second.append(std::forward<OrderT>(order));
                try {
                    locator_index_.emplace(order_id, OrderLocator{side, price, list_it});
                } catch (...) {
                    level_it->second.erase(list_it);
                    if (level_created && level_it->second.empty()) {
                        asks_.erase(level_it);
                    }
                    throw;
                }
            } catch (...) {
                if (level_created && level_it->second.empty()) {
                    asks_.erase(level_it);
                }
                throw;
            }
        }

        return true;
    }

    domain::ContractInstance contract_;
    BidMap bids_;
    AskMap asks_;
    LocatorIndex locator_index_;
};

} // namespace tickforge::order_book
