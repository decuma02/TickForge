#pragma once

#include "tickforge/domain/order.hpp"
#include "tickforge/domain/price_in_ticks.hpp"
#include "tickforge/domain/side.hpp"
#include <list>

namespace tickforge::order_book {

/// OrderLocator points directly to an order within a specific PriceLevel's std::list<Order>.
///
/// Enables O(1) removal and lookup from the OrderBook without scanning through price levels.
/// Contains:
/// - side: Buy or Sell
/// - price: Price level key in the corresponding ordered map
/// - order_iter: Direct iterator to the order node in std::list<domain::Order>
struct OrderLocator {
    domain::Side side;
    domain::PriceInTicks price;
    std::list<domain::Order>::iterator order_iter;
};

} // namespace tickforge::order_book
