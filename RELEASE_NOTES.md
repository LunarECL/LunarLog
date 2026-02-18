## What's New

### Copy-on-Write Optimization (#65)

Replace three hot-path copy operations with `shared_ptr<const T>` copy-on-write (COW) to eliminate per-log-entry heap allocations in the filter and template-cache paths.

**Before:** every log entry deep-copied filter rules, template placeholders, and tag sets.  
**After:** readers copy a `shared_ptr` (atomic refcount bump only); writers allocate a new snapshot (write-path is cold — config time only).

| Operation | Before | After |
|-----------|--------|-------|
| Per-sink filter check | `vector<FilterRule>` deep copy | `shared_ptr` refcount bump |
| Template cache hit | `vector<PlaceholderInfo>` deep copy | `shared_ptr` refcount bump |
| Global filter eval | Evaluated inside mutex | Snapshot outside mutex |
| Tag routing check | `set<string>` evaluated inside lock | Snapshot outside lock |

### Design

- `shared_ptr<const T>` — immutable snapshot; readers never block writers
- All mutators (`setFilter`, `addFilterRule`, `addOnlyTag`, etc.) use copy-modify-swap under lock
- `snapshotGlobalFilters()` private helper deduplicates the snapshot pattern
- All public APIs unchanged — internal representation only
- C++11 compatible throughout

## Highlights

- ✅ All platforms: GCC (C++11/14/17), Clang, AppleClang, MSVC
- ✅ 823 tests passing
- ✅ Review: R1 (M3/N2) → R2 (N1) → all resolved
