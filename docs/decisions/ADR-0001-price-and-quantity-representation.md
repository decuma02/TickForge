# ADR-0001: Integer Tick-Based Price and Quantity Representation

## Status
**ACCEPTED** (Amended during Phase 1A Engineering Review)

## Context
In financial matching engines and electronic exchanges, representing prices with floating-point types (`double`, `float`) leads to rounding inaccuracies, non-associative arithmetic, and non-deterministic comparisons. In discrete limit order books, price levels must be exact and discrete.

During the Phase 1A engineering review, two core architectural clarifications were established:
1. **Positivity vs. General Integer-Tick Representation**:
   Mathematically, an integer tick representation supports negative, zero, and positive ticks. In financial markets, certain contracts (e.g., negative crude oil futures during market dislocations, electricity/power futures, or synthetic options combo spreads) permit zero or negative prices. Therefore, `PriceInTicks` as a general domain primitive must represent arbitrary signed 64-bit integer ticks (`int64_t`) with overflow/underflow protection. Positivity is an instrument-specific validation constraint (e.g. for CME Globex ES outright futures), not an inherent property of the discrete integer-tick primitive itself. Instrument-specific positivity is enforced during order placement and product validation via `ProductSpecification::validate_price()`.
2. **Float & Decimal Conversion and Exact Tick Grid Validation**:
   When converting prices from external representations (such as fixed-point decimal points or floating-point input), silent rounding or clamping is strictly prohibited. The converter must validate exact alignment with the discrete tick grid. Floating-point inputs must also reject NaN, infinity, and out-of-range values.

## Decision
1. **Price Representation (`PriceInTicks`)**:
   - Represents prices as `PriceInTicks` wrapping a signed 64-bit integer (`int64_t`).
   - Supports the full range of signed integer ticks, allowing zero and negative ticks at the primitive level.
   - All comparisons and order book price level lookups are pure integer operations.
   - Core price calculations do not perform floating-point arithmetic.
   - Includes full overflow and underflow protection on addition and subtraction.
   - Provides exact conversion from decimal index points (`from_index_points`) and floating-point points (`from_double`).
   - Floating-point conversion explicitly rejects `NaN`, `Inf`, and any value not aligning exactly to the instrument tick grid within machine precision tolerance. Silent rounding and clamping are strictly forbidden.
2. **Instrument-Specific Price Validation**:
   - Outright futures contracts where prices must be strictly positive (such as ES) enforce positivity at the `ProductSpecification` and `Order` layer (`price.ticks() > 0`).
3. **Quantity Representation (`Quantity`)**:
   - Represent quantities as `Quantity` wrapping an unsigned 32-bit integer (`uint32_t`).
   - CME contracts are discrete units.
   - Zero-quantity values are rejected on non-sentinel construction.
   - Overflow and underflow are checked and throw explicit exceptions.
4. **Provisional Quantity Limits (`MaxTradeVol` / CME Tag 1140)**:
   - CME Tag 1140 (`MaxTradeVol`) represents the maximum order quantity allowed for an instrument.
   - The simulator default ES maximum trade volume of 2,999 is explicitly recorded as a **provisional simulator configuration**, derived from secondary CME Globex E-mini Equity Index documentation. We do not claim the current ES value is 2,999 without an actual ES Security Definition (`35=d`) message record.
   - Authoritative Tag 1140 values are dynamic security definition attributes received on CME MDP 3.0. TickForge parameterizes `max_trade_vol` in `ProductSpecification` as a provisional configuration until direct empirical `35=d` captures are secured.

## Consequences
- Guarantees 100% deterministic, exact comparisons across all architectures.
- Clean separation between internal general integer-tick storage and product-specific validation rules.
- Prevents fractional contract bugs and silent rounding errors.
