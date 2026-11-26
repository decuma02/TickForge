# TICKFORGE — Phased Engineering Roadmap

This roadmap defines the structured, iterative milestones for engineering the TICKFORGE electronic market simulation and matching engine.

---

## Phase 0: Foundation & Architectural Finalization (Completed)
- [x] Dockerfile with GCC-12, CMake, Ninja, GDB, Valgrind, and `perf`.
- [x] Google Test v1.14.0 and Google Benchmark v1.8.3 integration.
- [x] ASan (AddressSanitizer) and UBSan (UndefinedBehaviorSanitizer) build configurations.
- [x] GitHub Actions CI workflow targeting `dev` and `main` branches.
- [x] Phase 0 market research reconciliation and verified CME Globex ES outright rules.
- [x] Finalized Architectural Decision Records (ADRs 0001–0005).

---

## Phase 1: Core Domain, Limit Order Book, and Continuous Matching

### Phase 1A: Core Domain Value Types & Instrument Model (Current Milestone)
- [ ] Implement `PriceInTicks` (integer-tick wrapper with overflow-safe arithmetic and explicit conversions).
- [ ] Implement `Quantity` (`uint32_t` whole-contract lot representation).
- [ ] Implement `OrderId` (strongly typed 64-bit identifier).
- [ ] Implement `Side` (`Buy`, `Sell`).
- [ ] Implement `ProductSpecification` (tick size, contract multiplier, commodity code, min/max trade volume).
- [ ] Implement `ContractInstance` (individual traded contract, maturity date, trading status).
- [ ] Implement `Order` (stateful entity tracking initial, working, executed, reduced, and cancelled quantities).
- [ ] Focused unit tests verifying validation, failure behavior, and no silent clamping.

### Phase 1B: Price Levels & Order Storage Data Structures
- [ ] Implement `PriceLevel` containing FIFO queue (`std::list<Order>`).
- [ ] Implement `OrderLocator` pointing to price level and list iterator.
- [ ] Fast $O(1)$ order lookup table via `std::unordered_map<OrderId, OrderLocator>`.
- [ ] Implement `BookSide<Compare>` using `std::map<PriceInTicks, PriceLevel, Compare>`:
  - Bids: `std::map<PriceInTicks, PriceLevel, std::greater<PriceInTicks>>` (descending).
  - Asks: `std::map<PriceInTicks, PriceLevel, std::less<PriceInTicks>>` (ascending).
- [ ] Level 3 order book invariant validation (`no_cross`, quantity conservation).

### Phase 1C: Order Book Operations & Modify Semantics
- [ ] Placement of resting limit orders.
- [ ] Cancellation of resting orders (`CancelOrder`) with immediate locator cleanup.
- [ ] Explicit `ModifyOrder` command:
  - Quantity reduction: retains priority and updates quantity accounting invariant.
  - Quantity increase: loses priority, assigned new sequence number.
  - Price change: loses priority, relocated to target price level with new sequence number.
- [ ] Monotonically increasing priority sequence assigned per price-level entry.

### Phase 1D: Continuous FIFO Matching Engine
- [ ] Single-threaded, deterministic matching engine loop for single contract.
- [ ] Marketable order crossing against top-of-book passive resting orders.
- [ ] Passive execution price semantics (incoming aggressive order matches at resting limit price).
- [ ] Multi-fill generation for large aggressive orders spanning multiple price levels.
- [ ] Synchronous direct return of `ExecutionResult` containing generated fills.

### Phase 1E: Invariant Validation & Regression Test Harness
- [ ] Continuous runtime verification of order quantity accounting invariant across modifications.
- [ ] Randomized stress testing and differential testing for continuous matching.
- [ ] Micro-benchmarks for order insertion, modification, cancellation, and matching.

---

## Phase 2: Multi-Contract & Advanced Order Types
- [ ] Support multiple independent contract books concurrently.
- [ ] Time-in-Force qualifiers: Immediate-or-Cancel (IOC) and Fill-or-Kill (FOK).
- [ ] Post-Only order validation.

---

## Phase 3: Market Data & Replay
- [ ] Top-of-Book (BBO) and full Depth-of-Book (Level 2 / Level 3) event streams.
- [ ] Deterministic binary event recording and replay engine.

---

## Phase 4: Performance Optimization & Profiling
- [ ] Cache-line optimization and memory layout profiling with `perf`.
- [ ] Slab/pool allocators to eliminate dynamic memory allocations in the hot path.
- [ ] Benchmark comparison against baseline standard-library implementation.
