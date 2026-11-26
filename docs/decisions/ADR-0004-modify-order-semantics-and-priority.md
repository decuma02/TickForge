# ADR-0004: Modify-Order Semantics and Order Accounting Invariant

## Status
**ACCEPTED** (Amended during Phase 1A Engineering Review)

## Context
In CME Globex FIFO matching markets (tag `1142 = F`), orders resting in the queue may be modified by clients. Correct priority semantics and conservation of volume are essential:
- If an order reduces its quantity, should it keep its queue position?
- If an order increases its quantity or changes its price, should it retain or lose its priority?
- How is order quantity accounted for across complex lifecycles involving executions, reductions, increases, and cancellations without invalidating conservation invariants or silently mutating original submitted quantities?

During the Phase 1A engineering review, the original formulation of the invariant:
`InitialQuantity == ExecutedQuantity + RemainingQuantity + ReducedQuantity + CancelledQuantity`
was analyzed. When an order's quantity is increased, `InitialQuantity` would either have to be silently redefined/mutated (which corrupts the historical record of the original submitted order) or the equation becomes mathematically invalid.

## Decision
1. **Formal Order Accounting Model**:
   We establish an explicit volume conservation model preserving the distinction between all lifecycle quantities:
   - **Original Submitted Quantity ($Q_{\text{orig}}$)**: The quantity submitted at initial order placement (immutable).
   - **Cumulative Added Quantity ($Q_{\text{added}}$)**: Total volume added across all quantity-increasing modifications.
   - **Total Submitted Quantity ($Q_{\text{total}}$)**: $Q_{\text{total}} = Q_{\text{orig}} + Q_{\text{added}}$.
   - **Cumulative Executed Quantity ($Q_{\text{exec}}$)**: Total volume matched across all execution fills.
   - **Cumulative Reduced Quantity ($Q_{\text{reduced}}$)**: Total volume removed via quantity reductions without cancelling.
   - **Cancelled Quantity ($Q_{\text{cancelled}}$)**: Total volume cancelled upon explicit cancel or aggressive residual expiry.
   - **Remaining Working Quantity ($Q_{\text{rem}}$)**: Currently active resting contracts on the order book.

   The fundamental order conservation invariant is:
   $$Q_{\text{orig}} + Q_{\text{added}} = Q_{\text{exec}} + Q_{\text{rem}} + Q_{\text{reduced}} + Q_{\text{cancelled}}$$
   Or equivalently:
   $$Q_{\text{total}} = Q_{\text{exec}} + Q_{\text{rem}} + Q_{\text{reduced}} + Q_{\text{cancelled}}$$

2. **No Silent Redefinition**:
   `original_submitted_quantity` is immutable after order creation. Modifications that increase quantity accumulate into `cumulative_added_quantity`.

3. **Priority Retention Rules**:
   - **Quantity Reduction Only**: If the price remains identical and `new_working_qty < remaining_quantity`, the order **retains priority** in its price level queue.
   - **Quantity Increase**: If `new_working_qty > remaining_quantity`, the order **loses priority** and is assigned a new monotonically increasing engine sequence number, moving to the tail of the price level queue.
   - **Price Change**: Any price change causes the order to **lose priority** and be moved to the target price level's tail with a new sequence number.
   - **Combined Price Change and Quantity Reduction**: Still **loses priority**.
   - **Inactive Order Rejection**: Modifying, cancelling, or filling an inactive (filled/cancelled) order is rejected with an exception.

4. **Monotonically Increasing Priority Sequence**:
   - Priority is determined strictly by the engine-assigned 64-bit sequence counter, never by wall-clock timestamps.

## Consequences
- Accurately mirrors CME Globex FIFO priority rules.
- Fully deterministic order priority independent of platform clock skew.
- Mathematically sound and complete accounting across every permitted operation without loss of auditability.
