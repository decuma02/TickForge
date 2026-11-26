# TICKFORGE — Market Model & Exchange Research Report

## 1. Executive Summary & Approved Scope

TICKFORGE is a deterministic, single-threaded C++17 electronic market simulation and matching engine. The first target instrument is the **CME Globex E-mini S&P 500 (ES) outright futures contract**.

This document reconciles the Phase 0 market research against primary exchange specifications, records verified rule tags and evidence, outlines deliberate simulator assumptions, and defines the exact domain invariants.

---

## 2. CME Globex Market Rules & Verified Evidence

Every market rule in TICKFORGE is grounded in verified CME Globex specifications or explicitly recorded as an engineering assumption.

| Rule / Concept | Specification / Tag | Primary Source | Value / Evidence | Interpretation & Confidence |
| :--- | :--- | :--- | :--- | :--- |
| **Product Target** | CME Globex Outright Futures | CME Rulebook Chapter 358 (Rule 35801: contract specifications and $50.00 multiplier; Rule 35802.B: trading unit); CME Globex Product Reference | Symbol: `ES`<br>Commodity Code: `ES`<br>Underlying: S&P 500 Index | Outright quarterly futures contracts (March `H`, June `M`, September `U`, December `Z`). Confidence: High (Verified). |
| **Minimum Price Increment (Tick Size)** | Security Definition (`35=d`) Tag `969-MinPriceIncrement` | CME Rulebook Rule 35802.C; CME Product Specs & MDP 3.0 Reference | `0.25` index points ($12.50 per contract) | Discrete tick representation: 1 tick = 0.25 index points = $12.50 per contract ($50.00 multiplier per Rule 35801). Minimum price increment per Rule 35802.C. Confidence: High (Verified). |
| **Contract Multiplier & Trading Unit** | Tag `231-ContractMultiplier` | CME Rulebook Rule 35801 & Rule 35802.B | `$50.00` per index point (Trading unit: $50 × S&P 500 Stock Price Index) | Dollar value per contract = Price in index points × 50. Trading unit defined under Rule 35802.B. Confidence: High (Verified). |
| **Matching Algorithm** | Security Definition (`35=d`) Tag `1142-MatchAlgorithm` | CME Globex MDP 3.0 Documentation (Generic MDP tag definition) | Tag `1142 = F` (Generic MDP definition: FIFO / Price-Time Priority) | **Evidence distinction**: In generic CME Globex MDP 3.0 specifications, tag `1142 = F` is defined as standard FIFO (Price-Time Priority). However, authoritative evidence confirming that a specific ES outright instrument definition packet reports `1142 = F` requires an actual CME MDP 3.0 Security Definition (`35=d`) packet capture / message log for the contract. **Missing Evidence Explicitly Recorded**: No raw Security Definition message, specific SecurityID/instrument ID, or authoritative packet trace for ES outrights was captured in Phase 1A repository artifacts. FIFO matching for ES outrights is therefore adopted based on CME product guidelines, with the explicit caveat that instrument-level `35=d` Tag 1142 decoding is pending empirical capture. Confidence: Medium (Generic MDP definition verified; instrument-level packet trace missing). |
| **Maximum Order Size (`MaxTradeVol`)** | Security Definition (`35=d`) Tag `1140-MaxTradeVol` | CME Globex MDP 3.0 Reference Specification & E-mini Equity Index FAQ | `MaxTradeVol` (Tag 1140 represents maximum order quantity / single order volume ceiling) | Tag `1140-MaxTradeVol` specifies the maximum order size permitted for an instrument in CME Globex. While CME literature references a 2,999 contract ceiling for E-mini equity index outrights, **no actual ES Security Definition record (`35=d`) confirming 2,999 is currently present in this repository**. Therefore, 2,999 is explicitly classified as a **provisional simulator configuration** rather than a verified empirical constant. TICKFORGE parameterizes `max_trade_vol` in `ProductSpecification` to allow exact runtime configuration once empirical `35=d` captures are secured. Confidence: Provisional. |
| **Minimum Order Size (`MinTradeVol`)** | Security Definition (`35=d`) Tag `562-MinTradeVol` | CME Globex MDP 3.0 Reference | `1` contract | Whole-contract lot sizing. Fractional contracts are prohibited. Confidence: High (Verified). |
| **Order Modification Priority Rules** | CME Globex Matching Rules / FIFO Specification | CME Globex Documentation (Confluence / Client Systems Wiki) | • Quantity reduction: **Retains priority**<br>• Quantity increase: **Loses priority**<br>• Limit price change: **Loses priority**<br>• Account/clearing entity change: **Loses priority** (exchange rule; simulator defers account modification) | Modifying price or increasing quantity forces the order to re-queue with a new priority sequence number. Combined price change and quantity reduction still loses priority. Confidence: High (Verified). |
| **Trading Modes** | Pre-open Equilibrium vs. Continuous Trading | CME Globex Trading Schedule & Auction Rules | Pre-open: Indicative Opening Price (IOP) uncrossing auction.<br>Continuous: Continuous double auction with resting CLOB. | **Continuous trading matching is implemented first.** Pre-open equilibrium uncrossing auction logic is **deferred**. Confidence: High. |

---

## 3. Simulator Assumptions vs. Exchange Rules

To maintain high performance, deterministic reproducibility, and focused scope, TICKFORGE explicitly distinguishes exchange production infrastructure from simulator modeling choices:

| Domain Area | Exchange Production Environment | TICKFORGE Simulator Implementation | Justification |
| :--- | :--- | :--- | :--- |
| **Execution Architecture** | Distributed matching engines with iLink3 binary gateways and MDP 3.0 multicast | Single-threaded, deterministic in-memory execution loop | Eliminates thread synchronization, non-deterministic race conditions, and networking overhead. Guarantees 100% bit-exact replayability. |
| **Price Arithmetic** | Transmitted as mantissa and exponent (FIX scaled integers) | Pure integer ticks (`PriceInTicks` wrapping `int64_t`) | Eliminates floating-point non-determinism, division rounding errors, and epsilon checks. 1 tick = 0.25 points. General primitive supports signed ticks; instrument specifications validate positivity. |
| **Quantity Representation** | Whole contract counts | `Quantity` wrapping `uint32_t` | Whole contracts only. Prevents fractional contracts and provides well-defined non-overflow range for simulated volumes. |
| **Pre-Open Auction** | Indicative opening price (IOP) calculation and equilibrium uncrossing | **Deferred** | Initial phases focus strictly on continuous double-auction matching. |
| **Contract Expiry & Settlement** | Contract roll, daily settlement, final cash settlement | Domain metadata present; lifecycle transitions **deferred** | Domain models record expiration date and status, but automated expiry transitions and cash settlement are deferred to later phases. |
| **Event Logging & Persistence** | Distributed redundant write-ahead logs | Direct return of `ExecutionResult`; persistent binary event store **deferred** | Commands directly return results and generated fills. Persistent disk logging is deferred until core engine benchmarking is established. |

---

## 4. Formal Domain Invariants

The TICKFORGE engine enforces strict invariants across all price levels and order modifications:

### 4.1 Order Quantity Accounting Invariant
For every order $i$ throughout its lifecycle:
$$\text{TotalSubmittedQuantity}_i = \text{OriginalSubmittedQuantity}_i + \text{CumulativeAddedQuantity}_i$$
And:
$$\text{TotalSubmittedQuantity}_i = \text{CumulativeExecutedQuantity}_i + \text{RemainingQuantity}_i + \text{RemovedByReductionQuantity}_i + \text{CancelledQuantity}_i$$

Where:
- $\text{OriginalSubmittedQuantity}_i$: The original quantity submitted at initial placement (immutable).
- $\text{CumulativeAddedQuantity}_i$: Cumulative contracts added via upward `ModifyOrder` quantity increases.
- $\text{TotalSubmittedQuantity}_i$: Total volume submitted to the exchange for this order.
- $\text{CumulativeExecutedQuantity}_i$: Cumulative contracts matched and filled across all executions.
- $\text{RemainingQuantity}_i$: Currently working contracts resting on the order book.
- $\text{RemovedByReductionQuantity}_i$: Cumulative contracts removed via downward `ModifyOrder` quantity reductions without cancellation.
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
