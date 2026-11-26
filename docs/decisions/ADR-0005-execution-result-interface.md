# ADR-0005: Synchronous Execution Result Interface

## Status
**ACCEPTED**

## Context
When an incoming order is placed, modified, or cancelled, it may generate multiple execution fills or state updates.
The system requires an interface to communicate command outcomes back to the caller.

We evaluated whether to introduce a listener interface, observer pattern, or ring buffer versus returning command results directly.

## Decision
1. **Direct Synchronous Return**:
   - Commands (`PlaceOrder`, `ModifyOrder`, `CancelOrder`) return a concrete `ExecutionResult` or `CommandResult` struct directly to the caller.
   - An incoming aggressive order may generate multiple fills; `ExecutionResult` contains a vector of `Fill` records.
2. **No Premature Listeners or Ring Buffers**:
   - Do not introduce a listener interface, observer framework, ring buffer, or pub/sub bus in the initial implementation.
   - Keeping the return path synchronous and direct maintains deterministic simplicity and simplifies unit and integration testing.
3. **Future Extension**:
   - When event logging or market data feed generation is added in Phase 3, an output sink will be designed based on concrete profiling data.

## Consequences
- Minimal code footprint with zero virtual method dispatch overhead.
- Direct test assertions on command outcomes without mocking listeners.
