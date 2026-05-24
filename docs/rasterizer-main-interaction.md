# How `close_to_gl_rasterizer` Interacts with `main`

This document explains how `src/close-to-gl/close_to_gl_rasterizer.cpp` is used by `src/triangles/main.cpp`.

---

## 1) Where the integration starts

`main.cpp` includes the rasterizer API:

- `#include "../../include/software_rasterizer.hpp"`

This exposes:

- data structs (`RasterFrame`, `RasterVertex`, `LightingParams`),
- utility functions (`ndcToScreen`, `clamp01`, `evaluatePhongLighting`),
- culling helpers (`trianglePassesClipZ`, `isFrontFacingInNdc`),
- rasterization functions (`rasterizeSolidTriangle`, `rasterizeWireTriangle`, `rasterizeVertexPoint`),
- framebuffer management (`ensureRasterFrameSize`, `clearRasterFrame`).

---

## 2) Main branch that uses the rasterizer

Inside the render loop, `main` checks:

- `if (appearance.close2GlMode) { ... }`

When this flag is true, rendering is done on CPU through the rasterizer.  
When false, the standard GPU OpenGL path (`glDrawArrays`) is used directly.

So the rasterizer is active only in the **Close2GL CPU mode**.

---

## 3) Per-frame interaction sequence (CPU mode)

Each frame in CPU mode follows this order:

1. Build camera/model transforms in `main`:
  - `model = buildModelMatrix(...)`
  - `view = cameraViewMatrix(...)`
  - `projection = cameraProjectionMatrix(...)`
2. Prepare framebuffer via rasterizer:
  - `ensureRasterFrameSize(rasterFrame, framebufferWidth, framebufferHeight)`
  - `clearRasterFrame(rasterFrame, clearColor)`
3. For each triangle:
  - create `RasterVertex rv[3]`,
  - transform object-space position to eye-space and clip-space,
  - fill per-vertex raster fields (`invW`, `ndc`, `screen`, `posEye`, `normalEye`),
  - run culling helpers,
  - precompute Gouraud color if needed,
  - call one rasterization function based on draw mode.
4. Upload CPU color buffer to a GL texture with `glTexSubImage2D`.
5. Draw fullscreen quad with `quadProgram` to show the rasterized image.

---

## 4) Data ownership: who computes what

### `main.cpp` responsibilities

- controls mode switch (`close2GlMode`),
- computes transformation matrices,
- traverses mesh triangles,
- computes clip coordinates and `invW`,
- fills `RasterVertex` inputs,
- selects draw mode and shading mode,
- uploads raster output texture and displays it.

### `close_to_gl_rasterizer.cpp` responsibilities

- tests and writes final pixels (depth + color),
- performs perspective-correct interpolation,
- rasterizes triangles/lines/points,
- applies Phong lighting helper when requested,
- manages raster frame buffers and texture allocation,
- performs clip-Z and front-face culling checks.

---

## 5) Exact function-level call flow

For each triangle in CPU mode, `main` calls rasterizer functions in this pattern:

1. `ndcToScreen(...)`
  converts each vertex NDC xy to screen coordinates.
2. `trianglePassesClipZ(...)`
  skips triangles fully outside near/far range.
3. `isFrontFacingInNdc(...)`
  skips backfaces according to UI winding setting.
4. If shading mode is Gouraud (`1` or `2`):
  `evaluatePhongLighting(...)` is called per vertex to fill `rv[i].gouraudColor`.
5. Depending on draw mode:
  - points: `rasterizeVertexPoint(...)` three times (one per vertex),
  - wireframe: `rasterizeWireTriangle(...)`,
  - solid: `rasterizeSolidTriangle(...)`.

Inside those rasterization calls, the rasterizer then uses its internal helpers:

- `interpolatePerspectiveVec3(...)`
- `shadeAndWritePixel(...)`
- `edgeFunction(...)` (for solid fill coverage)

`main` does not call these internals directly.

---

## 6) Shading mode coupling between `main` and rasterizer

`main` passes `appearance.shadingMode` into rasterizer calls.  
Both files agree on the same meaning:

- default/other: flat `baseColor`
- `1`/`2`: Gouraud path using `gouraudColor`
  - in `main`, these per-vertex colors are precomputed via `evaluatePhongLighting`
  - in rasterizer, colors are interpolated across primitives
- `3`: Phong path
  - rasterizer interpolates `normalEye` + `posEye` per sample
  - rasterizer calls `evaluatePhongLighting` per sample

This shared contract is what keeps shading consistent between setup (`main`) and rasterization (`close_to_gl_rasterizer`).

---

## 7) Output handoff back to OpenGL

After CPU rasterization:

- `main` binds `rasterFrame.textureId`,
- uploads `rasterFrame.color` with `glTexSubImage2D`,
- draws a fullscreen quad.

So the rasterizer is not a separate display system; it is a **CPU renderer whose final image is presented through OpenGL**.

---

## 8) Lifecycle interactions

- Allocation:
  - `ensureRasterFrameSize` creates/recreates texture/buffers when window size changes.
- Per-frame clear:
  - `clearRasterFrame` resets color + depth.
- Shutdown:
  - `main` deletes `rasterFrame.textureId` before exit.

This gives clear ownership: rasterizer creates/uses the frame resources, while `main` orchestrates frame timing and final cleanup.