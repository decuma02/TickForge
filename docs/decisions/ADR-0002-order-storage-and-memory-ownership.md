# ADR-0002: Order Storage and Memory Ownership

## Status
**ACCEPTED** (Supersedes provisional representation)

## Context
A Level 3 Limit Order Book requires:
1. $O(1)$ fast lookup of orders by `OrderId` (for cancellation and modification).
2. $O(1)$ append to the tail of a price level queue (for new resting orders).
3. $O(1)$ removal from anywhere in the queue (for cancellations or executions).
4. Explicit ownership and clear iterator validity without introducing unnecessary raw-pointer ownership.

## Decision
1. **Order Ownership**:
   - Price levels own the order data. Within each price level, orders are stored in a `std::list<Order>`.
   - `std::list` provides iterator stability: insertion or deletion of other elements does not invalidate existing iterators to other elements in the list.
2. **Fast Lookup Table**:
   - Use `std::unordered_map<OrderId, OrderLocator>`.
   - `OrderLocator` contains:
     - `PriceInTicks price`: The price level of the order.
     - `Side side`: The side of the book.
     - `std::list<Order>::iterator order_iter`: An iterator directly referencing the order node in the price level's `std::list`.
3. **Iterator Validity and Safety**:
   - When an order is cancelled or completely filled, its node is erased from the `std::list<Order>` and the `OrderId` is removed from the lookup map.
   - No raw-pointer ownership is exposed across component boundaries.

## Consequences
- Clean, robust standard-library implementation with zero raw-pointer leaks.
- Avoids premature custom memory pools or intrusive lists before baseline benchmarking.
- Provides predictable $O(1)$ removals and FIFO order dispatch.
