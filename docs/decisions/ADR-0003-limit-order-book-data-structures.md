# ADR-0003: Price-Level Organization and Order Book Sides

## Status
**ACCEPTED** (Supersedes provisional representation)

## Context
A continuous double auction maintains two sides of the book: bids (buy orders) and asks (sell orders).
The top of the book (Best Bid and Best Ask) must be accessible in $O(1)$ time, and iterating price levels from best to worst must be natural and efficient.

## Decision
1. **Separate Ordered Maps**:
   - **Bids**: `std::map<PriceInTicks, PriceLevel, std::greater<PriceInTicks>>` (sorted descending; highest bid at `begin()`).
   - **Asks**: `std::map<PriceInTicks, PriceLevel, std::less<PriceInTicks>>` (sorted ascending; lowest ask at `begin()`).
2. **No Dense Arrays or Radix Trees Initially**:
   - Although tick prices are integers, the initial implementation does not use dense arrays, circular buffers, or radix trees.
   - Standard-library ordered maps provide clean boundary separation and handle arbitrary price spreads without memory blowup.
   - Optimizations will be benchmarked in Phase 4 before replacing ordered maps.

## Consequences
- Top-of-book lookup is $O(1)$ via `begin()`.
- Finding or inserting a price level is $O(\log P)$ where $P$ is the number of active price levels.
- Direct alignment with project principle: standard-library data structures first, benchmark before optimizing.
