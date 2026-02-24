# Event Input System Review and Refactor Suggestions

## Current Design Snapshot
- Hierarchical dispatcher chain: `EventDispatcher` feeds children with optional `exclusive_dispatcher` short-circuit.
- Events identified by `EventHeader { type, index, id }` plus opaque `uint64 data` reinterpreted per source.
- Specialized handlers (e.g., `MouseEvent`, `WindowEvent`) derive from `EventDispatcher` and decode `data` unions.
- Children stored in `SortedSet<EventDispatcher*>`; parent pointers maintained; exclusivity handled manually.

## Noted Pain Points / Risks
- Strong coupling to reinterpret-cast of `uint64 data`; unsafe layout assumptions, no validation of payload size.
- `RangeCheck` guards only type/id enums; no guard on `index` or `button` bounds; risk of out-of-range array writes.
- `exclusive_dispatcher` alters parent links and ownership semantics without clear lifecycle/ownership rules (raw pointers, no RAII).
- Dispatch order relies on `SortedSet` ordering of raw pointers; deterministic order unclear and may fluctuate.
- `OnEvent` always descends even when handler handled-but-continue (no explicit consume flag beyond Break/Continue); coarse result model.
- No per-source registration or filtering (every child sees every event of matching source); scale and performance concerns.
- Input mapping layer (`InputMapping.h`) empty; no abstraction for actions/contexts or device remapping.
- Thread-safety not defined; shared dispatcher tree mutated without guards.

## Incremental Improvements (Low Disruption)
1) Make payloads type-safe
   - Replace `uint64 data` casts with strongly typed structs or `std::variant`-like envelopes; ensure alignment/size assertions per event.
   - Provide helper decode functions to avoid repeated reinterpret casts.
2) Harden validation
   - Centralize bounds checks for `index`, `id`, and per-event subfields (e.g., mouse button range) before touching arrays.
   - Add debug assertions/logging when unknown ids/types arrive.
3) Clarify dispatch semantics
   - Define `EventProcResult` to include `Handled`/`StopPropagation` vs `Continue`; avoid overloading `Break` for errors.
   - Allow handlers to return `Handled` while still permitting bubbling if desired via explicit flags.
4) Deterministic ordering
   - Replace `SortedSet<EventDispatcher*>` with stable container (`std::vector` + explicit priority/order field) to avoid pointer-order nondeterminism.
5) Ownership and lifecycle
   - Document ownership expectations; consider using `std::unique_ptr` for children and observer-like weak refs for parents; avoid parent resets inside destructor that can cascade unexpected removals.
6) Logging/metrics hooks
   - Add optional tracing hooks around dispatch to measure latency and dropped events; useful for debugging complex input flows.

## Medium-Term Refactor Options
1) Event envelope + visitor
   - Define `struct Event { EventHeader header; std::variant<WindowEventData, MouseEventData, KeyboardEventData, JoystickEventData> payload; }`.
   - `OnEvent(const Event&)` prevents unsafe casts and enables compile-time exhaustiveness.
2) Contextual dispatch & subscriptions
   - Move from hierarchical broadcast to routing based on subscriptions (e.g., per-source/per-id listeners). A small dispatcher registry keyed by `InputEventSource` and `id` reduces work and avoids unnecessary fan-out.
   - Support scopes/contexts (UI vs gameplay) for input mapping.
3) Input mapping layer
   - Implement `InputMapping` to translate physical inputs to logical actions with rebinding, device profiles (PS/Xbox/Switch), and chord/axis handling.
   - Provide per-context maps with stack-based activation (push/pop) to keep focus-aware controls.
4) Thread safety
   - Guard tree mutations with mutex or require single-threaded ownership; document threading model.
5) Testability
   - Introduce a test harness for synthetic events; mock dispatchers to verify ordering, stopping rules, and state (e.g., pressed states).

## Long-Term Redesign Direction
- Shift to an event bus with typed channels: publishers push typed event objects; listeners subscribe with filters; allow async queues for high-frequency input.
- Separate responsibilities: decoding (OS → normalized events), mapping (normalized → actions), routing (actions → systems), UI focus (exclusive targets) handled as independent layers.
- Incorporate stateful input features: hold durations, double-tap recognition, axis dead-zones, and per-device calibration.

## Suggested Next Steps
- Choose a short-term goal: (1) strengthen validation + ordering + ownership, or (2) prototype typed `Event` envelope.
- Start with mouse/keyboard paths to validate the approach, add unit tests, and benchmark dispatch cost.
- Fill `InputMapping` with a minimal action-map implementation and integrate with dispatcher outputs.
