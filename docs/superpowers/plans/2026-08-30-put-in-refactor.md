# put_in Refactor Plan

**Goal:** Make `item::put_in` able to report that an insertion was refused, and give
all 48 call sites an explicit answer, so pocket enforcement can later be switched on
without items silently disappearing.

**Blocked by:** Phase 3 restrictions (landed), classic mode (landed).

## The problem this solves

`item::put_in` returns `void` and ignores `insert_item`'s result
(`src/item.cpp:1297`). Today that is harmless because `insert_item` never fails. The
moment enforcement is enabled, a refused insertion leaves the `detached_ptr` in the
caller's scope, it falls out of scope, and **the item is destroyed with no message**.

48 call sites, none of which use a return value:

```
item.cpp 10, iuse_actor.cpp 7, iuse.cpp 4, item_group.cpp 3,
computer_session.cpp 3, activity_handlers.cpp 3, ranged.cpp 2, dump.cpp 2,
weather.cpp 1, vehicle_part.cpp 1, vehicle_functions.cpp 1, vehicle.cpp 1,
profession.cpp 1, npctalk.cpp 1, npc.cpp 1, and the remainder singly elsewhere
```

## Global Constraints

- **Behaviour must not change in this refactor.** Enforcement stays off; every call
  site keeps doing exactly what it does today. This is a signature change plus
  explicit handling, nothing more.
- **No item may be dropped on the floor by omission.** Every site either consumes the
  returned pointer or deliberately re-inserts/discards with a comment saying why.
- **`[[nodiscard]]`** on the new signature, so the compiler finds any site that
  forgets. The compiler is the audit here, not my reading.
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- Judge the suite by pass/fail and case count, never assertion count.

---

### Task 1: Change the signature

**Files:** `src/item.h`, `src/item.cpp`

- [ ] `[[nodiscard]] detached_ptr<item> put_in( detached_ptr<item> &&payload );`
      returning an empty pointer on success, or the payload back on refusal.
- [ ] The two existing early-outs (null payload, self-insertion) already refuse; they
      must now return the payload rather than dropping it. **That is a live bug fix:
      today `put_in( self )` destroys the item.**
- [ ] Build and let the compiler enumerate every unhandled site.

### Task 2: Work through the call sites

**Files:** the 15+ listed above

- [ ] For each site, choose deliberately and comment anything non-obvious:
      - the item is guaranteed to fit (fresh container, known capacity) - assert the
        return is empty
      - a caller already has a failure path - route the item into it
      - no sensible failure path exists yet - keep today's behaviour and leave a
        `// TODO(pocket-enforcement):` marker naming what should happen
- [ ] Do this in batches by file, building between batches. A single 48-site sweep
      is unreviewable and hard to bisect.

### Task 3: Verification

- [ ] Full suite green, case count unchanged from 959.
- [ ] Grep for remaining `TODO(pocket-enforcement)` markers and list them; they are
      the exact work enforcement will need.
- [ ] Confirm no `put_in` result is discarded anywhere (the compiler guarantees this
      via `[[nodiscard]]`, but say so explicitly).

## Explicitly out of scope

**Turning enforcement on.** That is a separate commit needing a human at the keyboard,
because its failure mode is an item quietly vanishing during play and no test catches
that. This plan only makes enforcement *possible* to enable safely.
