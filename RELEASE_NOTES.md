## What's New

### LogEntry Lightweight Refactor (#66)

This release refactors `LogEntry` exception storage to reduce per-entry memory overhead on the non-exception logging path.

Before (`v1.22.x`):
- `LogEntry` always carried three exception strings:
  - `exceptionType`
  - `exceptionMessage`
  - `exceptionChain`
- Even when no exception was attached, these fields still existed per entry.

After (`v1.23.0`):
- `LogEntry` stores exception data only when present via:
  - `std::unique_ptr<detail::ExceptionInfo> exception`
- Exception access changed to:
  - `entry.exception->type`
  - `entry.exception->message`
  - `entry.exception->chain`
- Presence check:
  - `entry.hasException()` (or `if (entry.exception)` for compatibility)

| Area | Before | After |
|------|--------|-------|
| Exception storage | 3 inline string members on every log entry | Optional `unique_ptr<ExceptionInfo>` only when needed |
| Formatter guard | `!entry.exceptionType.empty()` | `entry.hasException()` |
| Hot path behavior | Placeholder exception object/string state always present | Non-exception path avoids exception object construction |

> ⚠️ **Breaking change for custom formatter users**: direct field names changed from `exceptionType/exceptionMessage/exceptionChain` to `exception->type/message/chain`.

### Design

- Kept C++11 compatibility by using existing `detail::make_unique` polyfill path.
- Made `LogEntry` move-only semantics explicit:
  - move ctor/assignment: `= default`
  - copy ctor/assignment: `= delete`
- Refactored emission path in `log_source.hpp`:
  - shared implementation with nullable exception pointer
  - separate non-exception overload to avoid placeholder exception allocation
- Updated all formatter surfaces consistently:
  - human-readable
  - JSON
  - compact JSON
  - XML
  - output template
- Regenerated `single_include/lunar_log.hpp` after code changes.

## Highlights

- ✅ All platforms: GCC (C++11/14/17), Clang, AppleClang, MSVC
- ✅ 823 tests passing, 0 failures
- ✅ Review: R1 (1 Minor, 2 Nits) → fixes applied → R2 (all fixed, no new findings)
