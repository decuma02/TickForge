# ADR-0004: Modify-Order Semantics and Monotonic Priority Sequence

## Status
**ACCEPTED**

## Context
In CME Globex FIFO matching markets (tag `1142 = F`), orders resting in the queue may be modified by clients. Correct priority semantics are essential for market fairness:
- If an order reduces its quantity, should it keep its queue position?
- If an order increases its quantity or changes its price, should it retain or lose its priority?
- How is priority tracked deterministically without relying on wall-clock time?

## Decision
1. **Explicit `ModifyOrder` Command**:
   - The engine provides an explicit `ModifyOrder` command that accepts `order_id`, `new_price`, and `new_quantity`.
2. **Priority Retention Rules**:
   - **Quantity Reduction Only**: If the price remains identical and `new_quantity < current_quantity`, the order **retains priority** in its price level queue.
   - **Quantity Increase**: If `new_quantity > current_quantity`, the order **loses priority** and is moved to the tail of the price level queue.
   - **Price Change**: Any price change causes the order to **lose priority** and be moved to the target price level's tail.
   - **Combined Price Change and Quantity Reduction**: Still **loses priority**.
   - **Account/Clearing Entity Modification**: Deferred.
3. **Monotonically Increasing Priority Sequence**:
   - Every order entry and every modification that loses priority is assigned a sequence number from a monotonically increasing 64-bit engine counter.
   - Queue priority is determined strictly by this engine-assigned sequence number, never by wall-clock timestamps.
4. **Quantity Accounting Invariant**:
   - When quantity is reduced via modification, the difference is tracked as `RemovedByReductionQuantity` to maintain the invariant:
     `InitialQuantity == CumulativeExecutedQuantity + RemainingQuantity + RemovedByReductionQuantity + CancelledQuantity`.

## Consequences
- Accurately mirrors CME Globex FIFO priority rules.
- Fully deterministic order priority independent of platform clock skew or time synchronization.
- Preserves accounting integrity across modifications.
