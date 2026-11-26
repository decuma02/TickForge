# ADR-0006: CI Branch Triggers for Dev and Main

## Status
**ACCEPTED**

## Context
Continuous Integration (CI) must run automated builds, unit tests, address sanitizers (ASan), undefined behavior sanitizers (UBSan), and benchmarks to prevent regressions.

Workflow triggers must correctly monitor active development branches while following GitFlow / branch hygiene conventions.

## Decision
1. **Pushes and Pull Requests**:
   - Update `.github/workflows/ci.yml` so that both `push` and `pull_request` trigger on branches `[ "main", "dev" ]`.
2. **Pull Request Semantics**:
   - Remember that `pull_request.branches` filters the pull request **target branch**, not its source branch. Feature branches targeting either `dev` or `main` will automatically trigger the CI workflow.

## Consequences
- Every pull request intended for `dev` or `main` is validated under ASan and UBSan in the container environment before merging.
- Direct pushes to `dev` and `main` are continuously monitored.
