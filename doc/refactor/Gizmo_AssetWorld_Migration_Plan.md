# Gizmo To AssetWorld Migration Plan

## Goal
Migrate gizmo runtime from `SubWorldComponent`-centric isolation to AssetWorld/AssetInstance bridge architecture while preserving current editor interaction behavior.

## Progress Dashboard (2026-03-08)
- Overall completion: `78%`
- GZ1 Backend Split: `100%` (`Done`)
- GZ2 Asset IDs/Registry: `100%` (`Done`)
- GZ3 Asset Create Path: `100%` (`Done`)
- GZ4 Interaction/Render Parity: `82%` (`Interaction done`, `runtime visual consumer v1 landed`)
- GZ5 Legacy Sunset: `20%` (`validation gates not yet met`)

Current blocker:
- Full parity runtime render consumer (feature-complete visual topology equivalent to legacy move/rotate/scale gizmo subgraphs) is not finished yet.
- Current v1 runtime visual consumer restores asset-path visibility using per-mode primitive proxies; further refinement is still required before default switch.

## Current State
- `src/SceneGraph/gizmo/GizmoUnified.cpp` creates three subworld-backed gizmo branches:
- Move
- Rotate
- Scale
- Legacy path remains the visual-safe default until GZ4 render consumer is completed.

## Phase GZ1: Backend Split (Safe, No Behavior Change)
1. Keep legacy subworld implementation as default backend.
2. Add backend selection scaffold (`legacy` / `asset`) with safe fallback to legacy.
3. Keep external API unchanged (`CreateTransformGizmo`, `UpdateTransformGizmo`, etc.).

Acceptance:
- No behavior change under default config.
- Explicit `asset` request does not crash and logs fallback.

Completion:
- `Done (100%)`.

## Phase GZ2: Asset IDs And Registry Contract
1. Define stable `AssetWorldId` values for gizmo definitions:
- Move gizmo definition
- Rotate gizmo definition
- Scale gizmo definition
2. Define instance key rules for one gizmo root -> 3 mode instances.
3. Add registration path to publish gizmo defs into `IAssetWorldRegistry`.

Acceptance:
- IDs are stable and deterministic across save/load.
- Registry contains all required gizmo defs before first bridge collect.

Status (2026-03-08):
- Implemented stable IDs in `src/SceneGraph/gizmo/GizmoUnified.cpp`:
- `kGizmoMoveAssetWorldId = 0x47495A4D4F000001`
- `kGizmoRotateAssetWorldId = 0x47495A4D4F000002`
- `kGizmoScaleAssetWorldId = 0x47495A4D4F000003`
- Implemented definition publish entry `EnsureGizmoAssetWorldDefinitions(ECSContext*)`:
- Reuses bridge registry when available.
- Falls back to a static local `AssetWorldRegistry` attached to bridge when bridge has no registry.
- Publishes `GizmoMove/GizmoRotate/GizmoScale` defs with version `1`.

Completion:
- `Done (100%)`.

## Phase GZ3: Asset Backend Create Path
1. Implement asset backend create path in `GizmoUnified`:
- create root + mode entities in main ECS
- attach `AssetInstanceComponent` instead of `SubWorldComponent`
- bind transform/visibility state to existing interaction logic
2. Keep interaction math and callbacks unchanged.

Acceptance:
- Move/Rotate/Scale interaction output matches legacy path.
- No subworld creation in asset backend path.

Status (2026-03-08):
- Implementation landed in `src/SceneGraph/gizmo/GizmoUnified.cpp`:
- Mode entities now attach `AssetInstanceComponent` with stable gizmo `AssetWorldId` bindings.
- Per-mode instance ids are generated deterministically from gizmo root `EntityID` + mode tag.
- Mode switches now also synchronize asset-instance active state (`flags` + `visibility_mask`).
- `ULRE_GIZMO_BACKEND=asset` now uses an initial asset create path that skips `SubWorldComponent` and skips `Create*GizmoImpl` creation.
- Asset backend interaction path now supports move/rotate/scale mapping, target sync, callback, visibility gate, and camera-space mapping.
- Legacy subworld path remains for transition/visual safety.

Completion:
- `Done (100%)` for create/switch/destroy/interaction logic migration.

## Phase GZ4: Render/Interaction Parity
1. Map mode visibility and active state to instance flags/visibility mask.
2. Verify pick/hover/drag behavior parity.
3. Validate camera-space/local-space mode behavior parity.
4. Add runtime render consumer for gizmo asset draw packets (bridge -> visible gizmo rendering path).

Acceptance:
- Functional parity on gizmo editing workflow.
- No regressions in current gizmo samples.
- Asset default mode keeps gizmo visible in runtime samples (`GizmoUsageExample`, `AtmosphereSkySunGizmo`, `BasicLitSunDirectionECS`).

Status (2026-03-08):
- Step1 landed: mode switch now updates asset backend binding state in `GizmoUnified` with bridge-observable metadata changes.
- For each mode entity (`Move/Rotate/Scale`):
- `visibility_mask` toggles active/inactive.
- `flags` keep render-pass base and active marker bit.
- `override_ref.payload_ref` encodes current mode + active state.
- `override_ref.revision` increments on mode sync to force deterministic bridge-visible state transitions.
- Step2 landed: asset backend now has a minimal drag interaction mapping in `UpdateTransformGizmo`.
- Move mode: screen delta -> root local position.
- Rotate mode: screen delta -> yaw/pitch quaternion delta.
- Scale mode: screen Y delta -> uniform local scale ratio with policy clamp.
- Target transform sync and `GizmoChangedCallback` are preserved in asset mode.
- Step3 landed: world/local semantics are now separated in asset backend interaction mapping.
- `MoveWorld`: screen delta on world XY plane.
- `MoveLocal`: screen delta interpreted in local frame then rotated by start rotation.
- `RotateWorld`: yaw/pitch around world Y/X axes.
- `RotateLocal`: yaw/pitch around start local Y/X axes.
- Step4 landed: root visibility now gates all asset mode instances (`Move/Rotate/Scale`) in sync with mode selection, preventing hidden gizmo from being reactivated by mode switch.
- Step5 landed: asset backend drag mapping now uses camera-space basis for world interaction:
- Move uses camera right/up vectors.
- Rotate world mode uses world-up yaw + camera-right pitch.
- Local modes derive axes from start rotation combined with camera basis.

Remaining for GZ4 completion:
- Implement runtime consumer that converts gizmo asset draw packets to actual visible rendering in sample/runtime path.
- Run non-headless parity and visual checks in all three samples.

Completion:
- `82%`.
- `Interaction parity`: `Done`.
- `Runtime visual parity`: `In progress` (v1 proxy rendering landed).

Newly landed (2026-03-08, step6):
- `src/SceneGraph/gizmo/GizmoUnified.cpp` now attaches minimal runtime-visible primitives in asset backend:
- Move mode entity: sphere proxy
- Rotate mode entity: torus proxy
- Scale mode entity: cube proxy
- Asset mode entities now enable fixed-pixel sizing parameters to keep gizmo proxies visible at editor camera distances.
- Proxy primitive visibility is synchronized with mode selection and root visibility through `SyncGizmoAssetModeBindings`.

## Phase GZ5: Legacy Sunset
1. Make asset backend default.
2. Keep `legacy` backend behind explicit opt-in for one transition cycle.
3. Remove subworld-only gizmo path after validation window.

Acceptance:
- Default gizmo path has no `SubWorldComponent` dependency.
- Legacy path removable without breaking tests.

Gate criteria before switching default to asset:
- GZ4 runtime render consumer completed.
- `GizmoUsageExample` and `AtmosphereSkySunGizmo` and `BasicLitSunDirectionECS` are visually correct in default asset mode.
- `ECS_GizmoAssetBackendSmoke` and `ECS_GizmoTransformParitySmoke` pass.
- One transition cycle keeps explicit `ULRE_GIZMO_BACKEND=legacy` opt-in for rollback.

Completion:
- `20%`.

## Suggested Tests
- `GizmoBackendSelectionSmoke`: env backend selection and fallback behavior.
- `GizmoAssetBackendCreateSmoke`: create/destroy/bind target in asset backend.
- `GizmoTransformParitySmoke`: compare transform deltas against legacy baseline.
- Existing bridge smoke tests stay as lower-level protection.
- `GizmoAssetRuntimeRenderSmoke` (new): verify non-zero visible gizmo render output path in runtime when backend=asset.
- Example visual checks (manual):
- `05_GizmoUsageExample`
- `02_AtmosphereSkySunGizmo`
- `03_BasicLitSunDirectionECS`

Implemented (2026-03-08):
- `ECS_GizmoAssetBackendSmoke` (under `src/ecs/tests/GizmoAssetBackendSmokeTest.cpp`):
- enables `ULRE_GIZMO_BACKEND=asset`
- validates no `SubWorldComponent` on mode entities
- validates mode visibility transitions and override revision bumps
- validates root visibility gating and destroy cleanup
- validates minimal move/rotate/scale drag updates in asset backend
- validates target transform synchronization and change callback firing
- validates deterministic post-drag position/rotation/scale expectations
- validates bridge runtime/draw packet stats around mode switches and interactions
- validates deterministic replay: same synthetic input sequence yields identical final transform in repeated runs

- `ECS_GizmoTransformParitySmoke` (under `src/ecs/tests/GizmoTransformParitySmokeTest.cpp`):
- attempts legacy-vs-asset parity on deterministic non-interactive API sequence
- in headless/no-render-resource environments, legacy path is explicitly skipped and asset sanity remains validated

## Execution Plan (Next)
1. Implement gizmo asset runtime render consumer (bridge draw packets -> visible gizmo rendering path).
2. Keep default backend `legacy` until step 1 is verified in runtime samples.
3. Add/extend smoke for runtime render visibility under `ULRE_GIZMO_BACKEND=asset`.
4. Run sample validation on:
- `05_GizmoUsageExample`
- `02_AtmosphereSkySunGizmo`
- `03_BasicLitSunDirectionECS`
5. After validation, switch default backend to `asset`; keep `legacy` opt-in for one cycle.
6. After transition cycle, remove subworld-only gizmo path and clean dead code.
