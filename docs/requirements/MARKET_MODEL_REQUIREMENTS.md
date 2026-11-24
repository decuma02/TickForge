# TICKFORGE — Market Model & Exchange Research Report

## 1. Executive Summary & Approved Scope

TICKFORGE is a deterministic, single-threaded C++17 electronic market simulation and matching engine. The first target instrument is the **CME Globex E-mini S&P 500 (ES) outright futures contract**.

This document reconciles the Phase 0 market research against primary exchange specifications, records verified rule tags and evidence, outlines deliberate simulator assumptions, and defines the exact domain invariants.

---

## 2. CME Globex Market Rules & Verified Evidence

Every market rule in TICKFORGE is grounded in verified CME Globex specifications or explicitly recorded as an engineering assumption.

| Rule / Concept | Specification / Tag | Primary Source | Value / Evidence | Interpretation & Confidence |
| :--- | :--- | :--- | :--- | :--- |
| **Product Target** | CME Globex Outright Futures | CME Rulebook Chapter 358; CME Globex Product Reference | Symbol: `ES`<br>Commodity Code: `ES`<br>Underlying: S&P 500 Index | Outright quarterly futures contracts (March `H`, June `M`, September `U`, December `Z`). Confidence: High (Verified). |
| **Minimum Tick Size** | Security Definition (`35=d`) Tag `969-MinPriceIncrement` | CME Product Specs & MDP 3.0 Reference | `0.25` index points | Discrete tick representation: 1 tick = 0.25 index points = $12.50 per contract ($50.00 multiplier). Confidence: High (Verified). |
| **Contract Multiplier** | Tag `231-ContractMultiplier` | CME Rulebook 35801 | `$50.00` per index point | Dollar value per contract = Price in index points × 50. Confidence: High (Verified). |
| **Matching Algorithm** | Security Definition (`35=d`) Tag `1142-MatchAlgorithm` | CME Globex MDP 3.0 Documentation & Databento Security Definition stream | Tag `1142 = F` (FIFO / Price-Time Priority) | Orders at the same discrete price level are matched strictly First-In, First-Out by engine sequence/priority timestamp. Confidence: High (Verified). |
| **Maximum Order Size (`MaxTradeVol`)** | Security Definition (`35=d`) Tag `1140-MaxTradeVol` | CME Globex E-mini Equity Index FAQ & MDP 3.0 Rule Specs | **2,999 contracts** for ES outrights (5,000 for calendar spreads) | Single order submission ceiling. Orders exceeding 2,999 contracts must be entered as multiple distinct entries. Note: In live environments, tag 1140 is dynamically received per contract security definition; historical/baseline value is 2,999. Confidence: High (Verified). |
| **Minimum Order Size (`MinTradeVol`)** | Security Definition (`35=d`) Tag `562-MinTradeVol` | CME Globex MDP 3.0 Reference | `1` contract | Whole-contract lot sizing. Fractional contracts are prohibited. Confidence: High (Verified). |
| **Order Modification Priority Rules** | CME Globex Matching Rules / FIFO Specification | CME Globex Documentation (Confluence / Client Systems Wiki) | • Quantity reduction: **Retains priority**<br>• Quantity increase: **Loses priority**<br>• Limit price change: **Loses priority**<br>• Account/clearing entity change: **Loses priority** (exchange rule; simulator defers account modification) | Modifying price or increasing quantity forces the order to re-queue with a new priority sequence number. Combined price change and quantity reduction still loses priority. Confidence: High (Verified). |
| **Trading Modes** | Pre-open Equilibrium vs. Continuous Trading | CME Globex Trading Schedule & Auction Rules | Pre-open: Indicative Opening Price (IOP) uncrossing auction.<br>Continuous: Continuous double auction with resting CLOB. | **Continuous trading matching is implemented first.** Pre-open equilibrium uncrossing auction logic is **deferred**. Confidence: High. |

---

## 3. Simulator Assumptions vs. Exchange Rules

To maintain high performance, deterministic reproducibility, and focused scope, TICKFORGE explicitly distinguishes exchange production infrastructure from simulator modeling choices:

| Domain Area | Exchange Production Environment | TICKFORGE Simulator Implementation | Justification |
| :--- | :--- | :--- | :--- |
| **Execution Architecture** | Distributed matching engines with iLink3 binary gateways and MDP 3.0 multicast | Single-threaded, deterministic in-memory execution loop | Eliminates thread synchronization, non-deterministic race conditions, and networking overhead. Guarantees 100% bit-exact replayability. |
| **Price Arithmetic** | Transmitted as mantissa and exponent (FIX scaled integers) | Pure integer ticks (`PriceInTicks` wrapping `int64_t`) | Eliminates floating-point non-determinism, division rounding errors, and epsilon checks. 1 tick = 0.25 points. |
| **Quantity Representation** | Whole contract counts | `Quantity` wrapping `uint32_t` | Whole contracts only. Prevents fractional contracts and provides well-defined non-overflow range for simulated volumes. |
| **Pre-Open Auction** | Indicative opening price (IOP) calculation and equilibrium uncrossing | **Deferred** | Initial phases focus strictly on continuous double-auction matching. |
| **Contract Expiry & Settlement** | Contract roll, daily settlement, final cash settlement | Domain metadata present; lifecycle transitions **deferred** | Domain models record expiration date and status, but automated expiry transitions and cash settlement are deferred to later phases. |
| **Event Logging & Persistence** | Distributed redundant write-ahead logs | Direct return of `ExecutionResult`; persistent binary event store **deferred** | Commands directly return results and generated fills. Persistent disk logging is deferred until core engine benchmarking is established. |

---

## 4. Formal Domain Invariants

The TICKFORGE engine enforces strict invariants across all price levels and order modifications:

### 4.1 Order Quantity Accounting Invariant
For every order $i$ throughout its lifecycle:
$$\text{InitialQuantity}_i = \text{CumulativeExecutedQuantity}_i + \text{RemainingQuantity}_i + \text{RemovedByReductionQuantity}_i + \text{CancelledQuantity}_i$$

Where:
- $\text{InitialQuantity}_i$: The original quantity submitted at placement.
- $\text{CumulativeExecutedQuantity}_i$: Cumulative contracts matched and filled across all executions.
- $\text{RemainingQuantity}_i$: Currently working contracts resting on the order book.
- $\text{RemovedByReductionQuantity}_i$: Cumulative contracts removed via successful `ModifyOrder` (quantity reductions) without cancellation.
- $\text{CancelledQuantity}_i$: Contracts explicitly removed via `CancelOrder` or remaining unexecuted upon aggressive order cancellation.

### 4.2 Book Price Priority & Spread Invariant
When both bid and ask sides contain resting orders:
$$\text{BestBidPrice} < \text{BestAskPrice}$$
The order book is never crossed or locked in continuous trading. Any marketable incoming order matches resting liquidity until exhausted or non-marketable.

### 4.3 Price Level Aggregation Invariant
For any discrete `PriceLevel` $L$:
$$\text{TotalQuantity}(L) = \sum_{k \in \text{Orders}(L)} \text{RemainingQuantity}_k$$
And:
$$\text{OrderCount}(L) = |\text{Orders}(L)|$$

### 4.4 Engine Priority Sequence Invariant
Every state-altering event (new order, modify order with lost priority) receives a monotonically increasing engine sequence number $S$:
$$S_{t+1} > S_t$$
Resting orders within each `PriceLevel` are sorted strictly by $S$:
$$\forall k < m \implies S_k < S_m$$
Priority ordering is independent of system wall-clock jitter.
