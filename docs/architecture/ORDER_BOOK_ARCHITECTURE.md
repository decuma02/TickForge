# Order Book Data Structures Architecture (Phase 1B)

## 1. Overview

The `tickforge::order_book` module provides the core Level 3 (L3) limit order book storage foundation for TickForge.
It manages resting limit orders for an individual `ContractInstance`, maintaining bid and ask sides, FIFO priority per price level, aggregate level quantities, and fast $O(1)$ order lookups by `OrderId`.

## 2. Component Hierarchy & Ownership

```
OrderBook (owns contract instance, bid map, ask map, and locator index)
 ├── bids_ : std::map<PriceInTicks, PriceLevel, std::greater<PriceInTicks>>
 │    └── PriceLevel (owns orders resting at price P via std::list<Order>)
 ├── asks_ : std::map<PriceInTicks, PriceLevel, std::less<PriceInTicks>>
 │    └── PriceLevel (owns orders resting at price P via std::list<Order>)
 └── locator_index_ : std::unordered_map<OrderId, OrderLocator>
      └── OrderLocator (Side side, PriceInTicks price, std::list<Order>::iterator order_iter)
```

### PriceLevel
- Represents a single price and owns orders via `std::list<domain::Order>`.
- Orders are strictly queued in FIFO arrival order.
- Maintains `total_quantity()` as the sum of `remaining_quantity()` of all resting orders at this price.
- Supports $O(1)$ tail appends and $O(1)$ order removal using a valid `std::list<Order>::iterator`.

### OrderLocator
- Lightweight locator holding the side, price, and `std::list<domain::Order>::iterator`.
- Provides direct dereferencing of resting order nodes without linear list searches or price-level map traversals.

### OrderBook
- Holds separate ordered maps for bids and asks:
  - Bids: `std::map<PriceInTicks, PriceLevel, std::greater<PriceInTicks>>` (descending: best bid at `begin()`).
  - Asks: `std::map<PriceInTicks, PriceLevel, std::less<PriceInTicks>>` (ascending: best ask at `begin()`).
- Fast lookup table: `std::unordered_map<OrderId, OrderLocator>`.
- Non-copyable and non-movable to preserve absolute iterator stability for `order_iter` in locators.

## 3. Order Book Invariants

The `OrderBook::check_invariants()` method validates the following Level 3 consistency rules:
1. **Quantity Conservation**: For every `PriceLevel`, `total_quantity()` equals the exact sum of `remaining_quantity()` for all resting orders in that level.
2. **Index Completeness**: The number of entries in `locator_index_` matches the total count of resting orders across all price levels.
3. **Pointer Integrity**: Every entry in `locator_index_` maps to a live order with identical `OrderId`, `Side`, and `PriceInTicks`.
4. **Empty Level Elimination**: No empty `PriceLevel` remains in `bids_` or `asks_`. As soon as the final order is removed from a price level, that level is pruned from the map.
5. **Side Separation & Ordering**: Bids are strictly descending; asks are strictly ascending. Duplicate order IDs across sides or levels are rejected.

## 4. Complexity Expectations

| Operation | Time Complexity | Notes |
| :--- | :--- | :--- |
| Best Bid / Ask Inspection | $O(1)$ | Direct dereference of `bids_.begin()` or `asks_.begin()` |
| Lookup by `OrderId` | $O(1)$ average | Hash lookup in `locator_index_` |
| Append to existing `PriceLevel` | $O(\log P)$ | Map lookup of level + $O(1)$ list append and hash insert |
| Append creating new `PriceLevel`| $O(\log P)$ | Map insertion + $O(1)$ list append and hash insert |
| Removal by `OrderId` | $O(\log P)$ | Hash lookup + $O(1)$ list erase + map erase if level emptied |

*(Where $P$ is the number of active price levels on the side)*

## 5. Scope Boundaries & Deferred to Phase 1C

In accordance with Phase 1B specifications, matching engine logic is strictly excluded:
- No crossing or execution logic is executed during order insertion.
- Incoming orders that cross the opposite side are stored if inserted; matching and marketable order handling belong to Phase 1C.
- Order modifications (`ModifyOrder` cancel/replace semantics) are handled in Phase 1C.
