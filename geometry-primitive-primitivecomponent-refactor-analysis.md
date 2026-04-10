# Geometry / Primitive / PrimitiveComponent Responsibility Analysis

Date: 2026-04-10 (updated)

## 1. Background

The current ECS rendering flow shows a behavior mismatch in scenes like:

- 10 geometry variants
- 100 entity material-instance configurations

Observed result: only about 10 MI entries are effectively used.

This is not a single-callsite bug. It is a responsibility boundary issue across Geometry, Primitive, PrimitiveComponent, and ECS material instance upload.

## 2. Current Model (As Implemented)

### 2.1 Primitive currently mixes two responsibilities

Primitive currently stores both:

- geometry draw carrier state (geometry, draw range, VIL path)
- mutable per-instance material binding state (material/domain/mi_id/MIT)

This makes a shared Primitive act like a shared mutable material-instance container.

### 2.2 Render collect mutates Primitive per entity

Render collect resolves material slot and then writes it back to Primitive via BindMaterialSlot.

When many entities share one Primitive, later entity updates overwrite prior slot state.

### 2.3 MI upload dedups by Primitive pointer

MaterialInstanceAssignmentBuffer collects unique primitives (Primitive*) and uploads MI by that key.

So if 100 entities reference 10 Primitive pointers, the MI path naturally collapses to 10 effective entries.

### 2.4 PrimitiveComponent MI override is not integrated as first-class key

PrimitiveComponent has mi_id_override API, but this path is not treated as the primary dedup key in ECS upload.

Result: setting override does not guarantee unique per-entity MI behavior.

## 3. Root Cause Summary

The root cause is key mismatch:

- semantic intent key should be per-entity material slot identity
- actual runtime key is mostly Primitive pointer identity

Therefore, shared Primitive + per-entity MI is currently inconsistent by design.

## 4. Target Responsibility Model

### 4.1 Geometry

Owns only mesh/topology/buffers.

### 4.2 Primitive

Represents reusable geometry draw prototype and static compatibility data.
Should not be the mutable owner of per-entity MI slot selection.

### 4.3 PrimitiveComponent / RenderItem

Owns per-entity resolved material slot snapshot:

- material_template
- domain
- mi_id
- MIT payload (or MIT reference)
- render/material preset

### 4.4 MI upload path

Dedup/upload key should be material-slot identity (domain + mi_id + material_template, optionally MIT hash), not Primitive*.

## 5. Recommended Refactor Plan

## Phase A (minimal behavior fix, low API risk)

1) Add per-item resolved material slot snapshot to RenderItem/PrimitiveRenderItem.
2) In RenderPrimitiveCollectSystem, write resolved slot to RenderItem instead of mutating shared Primitive.
3) In MaterialInstanceAssignmentBuffer, dedup by material-slot key, not Primitive*.

Expected result:

- 10 shared Geometry/Primitive can remain
- 100 per-entity MI slots can coexist and upload correctly

Status: Completed

- Done: per-item resolved material slot snapshot is in the render item path.
- Done: collect flow consumes resolved slot data without relying on shared `Primitive*` mutation as the identity key.
- Done: MI assignment dedup/write path is aligned to resolved domain + mi_id flow.
- Verified: `08_PBRSpheresECS` and `14_PBRColor3DSpheresECS` reached stable `mode_seen=1` and `mi_direct=1, mi_fallback=0` in descriptor binding summary logs.

## Phase B (alignment and cleanup)

1) Reduce Primitive mutable MI state responsibilities.
2) Narrow BindMaterialSlot usage to explicit prototype-level or deferred-material setup only.
3) Make PrimitiveComponent override path explicit and validated in resolve flow.

Status: In progress

- Done: override consumption and resolved-slot-first behavior are wired in the ECS path.
- Done: collect-time `BindMaterialSlot` skipping is now limited to true domain-direct instance slots (`domain != nullptr && mi_id >= 0`).
- Done: Phase B next-step telemetry added in `PrimitiveBatchPipeline` to classify domain-direct fallback reasons (`fallback_no_snapshot`, `fallback_no_material`, `fallback_no_domain`, `fallback_no_mi`).
- Done: first cleanup slice landed: resolved-slot draw validity is decoupled from instance-id validity (material+domain can use resolved-slot path even when `mi_id == -1`; instance-indexed paths still gate on `mi_id >= 0`).
- Ongoing: remove/contain residual shared `Primitive` mutable material side-effects from non-transition call paths.
- Ongoing: tighten `BindMaterialSlot` usage boundary and document allowed usage sites.
- Validated: `08_PBRSpheresECS` sky sphere regression was recovered after a Phase B cleanup edge case where semantic resolve produced `domain != nullptr` but `mi_id == -1`; those non-instanced semantic slots must still bind back to the shared `Primitive` until the fallback dependency is fully removed.

## Phase C (API hardening)

1) Introduce explicit data types:
   - PrimitivePrototype (geometry-centric)
   - EntityMaterialBinding (instance-centric)
2) Deprecate ambiguous APIs where shared object mutation implies per-entity state.

Status: In progress (transitional implementation active)

- Done: domain-owned MI/MIT GPU buffer support and dirty-range upload API were added in `MaterialResourceDomain`.
- Done: descriptor binding now prefers domain-direct MI/MIT SSBO and retains legacy fallback for compatibility.
- Done: runtime transitional diagnostics added (`DomainDirectSummary`) including MIT attempt/semantic-off/reason stats.
- Ongoing: finalize full migration to explicit entity binding types and retire legacy fallback path.

## 5.1 Progress Snapshot (2026-04-10)

- Completed: behavior regression fixed for shared-primitive + per-entity MI identity collapse.
- Completed: transition-mode verification for MI direct bind on both key regression samples.
- Partially completed: MIT direct path verified on `08`; `14` currently reports `mit_attempt=0` with `mit_semantic_off` (MIT semantic not requested), which is expected for current material contract.
- Completed: sky-sphere regression fix validated on `08`; batch summary returned to `items=101 resolved_slot=100 primitive_slot=1 batches=2` and descriptor binding remained on the direct path (`mi_direct=1, mi_fallback=0, mit_direct=1, mit_fallback=0`).
- Completed: fresh rerun validation on `14` after the Phase B fix path shows stable direct binding with no skipped draw items in captured logs (`mode_seen=1 batches=2 mi_direct=1 mi_fallback=0 mit_direct=0 mit_fallback=0 mit_attempt=0 mit_semantic_off=2`).
- Completed: first Phase B cleanup slice validated on both samples. `08` and `14` now report `resolved_slot=101 primitive_slot=0 fallback=0` with `batches=2` and `item skipped (no draw call)=0`.
- Pending: Phase B cleanup completion and Phase C final API deprecation cutover.

## 5.3 Next Step Entry (Phase B fallback attribution)

To continue cleanup safely, this pass added fallback attribution counters in `PrimitiveBatchPipeline` without changing draw behavior.

What was added:

- `BatchPipeline::ResolvedSlotSummary` now reports why an item did not take resolved-slot domain-direct path:
   - `fallback_no_snapshot`
   - `fallback_no_material`
   - `fallback_no_domain`
   - `fallback_no_mi`

Why this is the next step:

- Phase B needs to shrink primitive-state fallback paths incrementally.
- Attribution counters let us separate expected fallback (e.g. non-instanced semantic slots) from accidental fallback (missing/invalid snapshot data).
- This provides a concrete gate for each subsequent cleanup patch.

Current reading after this cleanup:

- `08_PBRSpheresECS`: `resolved_slot=101 primitive_slot=0 fallback=0`, `batches=2`, `item skipped (no draw call)=0`.
- `14_PBRColor3DSpheresECS`: `resolved_slot=101 primitive_slot=0 fallback=0`, `batches=2`, `item skipped (no draw call)=0`.
- Descriptor binding remains direct-path stable in both samples (`mi_direct=1, mi_fallback=0`; `14` keeps `mit_attempt=0, mit_semantic_off=2` as expected by current contract).

## 5.2 Phase B Regression Note: Non-instanced semantic slots

During Phase B cleanup, `RenderPrimitiveCollectSystem` temporarily skipped `Primitive::BindMaterialSlot(...)` whenever domain-direct mode was enabled.

That was too broad.

The failure case was the sky sphere in `08_PBRSpheresECS`:

- semantic resolve succeeded
- resolved slot had valid material/domain
- resolved slot carried `mi_id = -1`
- collect path skipped primitive binding
- later batch path did not treat the item as a resolved-slot-backed domain-direct instance
- result: `material=null`, `pipeline=null`, and the sky sphere never entered a draw batch

Refined rule:

- skip `BindMaterialSlot(...)` only for true domain-direct instance slots (`domain != nullptr && mi_id >= 0`)
- if semantic resolve returns a non-instanced slot (`mi_id == -1`), still bind the slot back to the `Primitive` until the remaining primitive-state dependency is removed from the draw path

Why this matters:

- `domain != nullptr` alone does not imply that later stages can consume the slot without shared-primitive fallback state
- Phase B cleanup must preserve non-MI / non-instanced semantic materials while shrinking primitive-owned mutable state
- this is now an explicit regression gate for future cleanup passes

## 6. Why previous workaround was insufficient

Workaround used in one sample: create 100 Primitive objects for 100 entities.

It works by bypassing the key mismatch, but duplicates object layer responsibility and weakens the original design goal of geometry reuse.

So it is a tactical workaround, not an architectural fix.

## 7. Risks and Validation Checklist

### Risks

- batch key changes can alter sorting/grouping behavior
- descriptor binding assumptions may rely on Primitive identity
- hidden dependencies on Primitive::BindMaterialSlot side effects

### Validation checklist

1) Scene with 10 geometry + 100 MI must produce 100 effective MI uploads.
2) Shared-Primitive scenes must remain visually stable frame-to-frame.
3) Existing examples without MI overrides must remain unchanged.
4) MIAB diagnostics should report expected unique material-slot count.

Current validation status:

- (1) Done on regression samples (`08`, `14`) via runtime transition diagnostics.
- (2) Done for current transition patch set (no instability observed in verification runs).
- (3) Partially done: `14` rerun after the sky fix path shows no `item skipped (no draw call)` lines in the captured run logs; broader example sweep is still pending.
- (4) Replaced by transition-era descriptor summary diagnostics for this stage; MIAB-only metrics are no longer the sole source of truth in domain-direct mode.
- Additional gate: non-instanced semantic materials must remain drawable under domain-direct mode; current `08` verification shows the expected second batch for the sky sphere.

## 8. Open discussion points

1) Should MIT be part of dedup key always, or only when texture-array mode is enabled?
2) Should mi_id_override remain API, or be replaced with explicit EntityMaterialBinding data?
3) Do we want Primitive-level mutable material fields at all after refactor?
4) Can we stage migration by feature flag to de-risk sample regressions?

---

Prepared for follow-up design discussion.
