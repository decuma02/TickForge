# ADR-0001: Integer Tick-Based Price and Quantity Representation

## Status
**ACCEPTED** (Supersedes provisional representation)

## Context
In financial matching engines and electronic exchanges, representing prices with floating-point types (`double`, `float`) leads to rounding inaccuracies, non-associative arithmetic, and non-deterministic comparisons. In discrete limit order books, price levels must be exact and discrete.

For the initial CME Globex E-mini S&P 500 (ES) outright futures target:
- Prices move in discrete ticks of 0.25 index points ($12.50 per tick for a $50 contract multiplier).
- Quantities are strictly whole contracts.
- Arithmetic must be overflow-safe and reject negative prices or zero quantities.

## Decision
1. **Price Representation**:
   - Represent prices as `PriceInTicks` wrapping a signed 64-bit integer (`int64_t`).
   - All comparisons and order book price level lookups are pure integer operations.
   - Core price calculations do not perform floating-point arithmetic.
   - Conversions to/from decimal display points (e.g. `5000.25`) require an explicit conversion function using instrument tick size and validate integer divisibility.
2. **Quantity Representation**:
   - Represent quantities as `Quantity` wrapping an unsigned 32-bit integer (`uint32_t`).
   - CME contracts are discrete units.
   - Zero-quantity values are rejected on construction.
3. **No Silent Clamping**:
   - Arithmetic overflow and underflow must be detected explicitly. Violations throw an exception or return an explicit error rather than silently saturating.

## Consequences
- Guarantees 100% deterministic, exact comparisons across all architectures.
- Clean separation between internal integer tick storage and human-readable decimal representations.
- Prevents fractional contract bugs.
