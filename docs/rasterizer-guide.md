# `close_to_gl_rasterizer.cpp` Reader Guide

This document explains the functions in `src/close-to-gl/close_to_gl_rasterizer.cpp` in a study-friendly way.

## What this file does

This file is the software rasterization core of the project. It:

- receives transformed vertices (`RasterVertex`) in screen/NDC space,
- applies culling checks,
- rasterizes triangles/edges/points into a CPU framebuffer (`RasterFrame`),
- runs depth testing and shading,
- prepares data for display through an OpenGL texture.

---

## Pipeline overview

1. Geometry arrives with per-vertex data (`screen`, `ndc`, `normalEye`, `posEye`, `gouraudColor`, `invW`).
2. Optional culling tests remove invisible triangles.
3. Rasterization happens in one of three modes:
  - solid triangle fill,
  - wireframe edges,
  - point rendering.
4. Each sample is depth-tested and written to the framebuffer.
5. The framebuffer texture is used for final display.

---

## Internal helpers (`namespace { ... }`)

### `edgeFunction(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &p)`

Computes the 2D cross product magnitude/sign:

- gives the signed area of the parallelogram formed by vectors `(a -> b)` and `(a -> p)`,
- used to determine whether a pixel lies inside a triangle,
- also used to compute barycentric weights.

---

### `shadeAndWritePixel(RasterFrame &frame, int x, int y, float ndcDepth, const glm::vec3 &rgb)`

Final pixel write stage. It:

- rejects out-of-bounds pixels,
- rejects depth outside clip range (`ndcDepth` not in `[-1, 1]`),
- converts NDC depth to `[0, 1]`,
- performs depth test (`smaller` depth wins),
- writes clamped RGB to RGBA8 color buffer and updates depth buffer.

---

### `interpolatePerspectiveVec3(...) - REVIEW`

Performs perspective-correct interpolation of any `vec3` attribute using barycentric weights and each vertex `invW`.

Why needed:

- linear interpolation in screen space is not perspective correct,
- this fixes distortion for interpolated color, normal, and position.

Used by:

- Gouraud color interpolation,
- Phong normal/position interpolation.

---

### `rasterizeWireEdge(...)`

Rasterizes one line segment between two vertices.

How:

- computes `dx`, `dy`, picks number of steps from max axis span,
- walks parameter `t` from 0 to 1,
- interpolates pixel position and depth,
- computes color by shading mode,
- writes each sample using `shadeAndWritePixel`.

Shading behavior:

- mode `1`/`2`: interpolates `gouraudColor` (perspective-correct),
- mode `3`: interpolates normal + position and evaluates Phong per sample,
- otherwise uses `baseColor`.

---

## Public utility functions

### `clamp01(const glm::vec3 &c)`

Clamps each color channel to `[0, 1]`.

---

### `ndcToScreen(const glm::vec3 &ndc, int width, int height)`

Maps NDC coordinates (`[-1, 1]`) to screen coordinates (`[0, width-1]`, `[0, height-1]`).

---

### `evaluatePhongLighting(...)`

Computes Phong lighting in eye space:

- ambient term,
- diffuse term (`max(dot(N, L), 0)`),
- optional specular term (`pow(max(dot(R, V), 0), shininess)`).

Returns clamped RGB.

---

## Framebuffer management

### `ensureRasterFrameSize(RasterFrame &frame, int width, int height)`

Ensures framebuffer storage and GL texture match target size.

It:

- updates `frame.width` and `frame.height`,
- resizes CPU color/depth arrays,
- recreates texture when needed,
- configures nearest filtering and clamp wrap mode,
- allocates texture storage with `GL_RGBA8`.

---

### `clearRasterFrame(RasterFrame &frame, const glm::vec4 &clearColor)`

Clears framebuffer contents:

- fills all color pixels with `clearColor` converted to bytes,
- resets depth buffer to `1.0f` (far plane).

---

## Rasterization entry points

### `rasterizeSolidTriangle(...)`

Filled triangle rasterizer using edge functions.

Steps:

1. Copies input vertices.
2. Computes signed area and rejects degenerate triangles.
3. Enforces consistent winding (swap if negative area).
4. Computes triangle bounding box, clamped to framebuffer.
5. For each pixel center in the box:
  - computes edge values,
  - rejects outside pixels,
  - computes barycentric weights,
  - interpolates depth,
  - computes color from shading mode,
  - writes pixel with depth test.

Shading modes:

- mode `1`/`2`: uses perspective-correct interpolation of `gouraudColor`,
- mode `3`: interpolates `normalEye` and `posEye`, then evaluates Phong,
- otherwise uses `baseColor`.

---

### `rasterizeWireTriangle(...)`

Draws a wireframe triangle by calling `rasterizeWireEdge` for the three edges:

- `(v0, v1)`,
- `(v1, v2)`,
- `(v2, v0)`.

---

### `rasterizeVertexPoint(...)`

Draws a vertex as a square point primitive centered on rounded screen coordinates.

It:

- computes point color by shading mode,
- uses `pointSize` to derive square radius,
- writes each covered pixel with depth testing.

---

## Culling helpers

### `trianglePassesClipZ(const glm::vec3 &n0, const glm::vec3 &n1, const glm::vec3 &n2)`

Rejects triangle only if all vertices are:

- behind near plane (`z < -1`), or
- beyond far plane (`z > 1`).

If triangle intersects clip volume in Z, it passes.

---

### `isFrontFacingInNdc(const glm::vec3 &n0, const glm::vec3 &n1, const glm::vec3 &n2, bool frontFaceClockwise)`

Backface test in NDC XY:

- computes signed 2D area,
- degenerate area returns false,
- compares sign to configured front-face winding (`clockwise` or not).

---

## Quick mode reference

- `shadingMode == 0` (or default branch): flat `baseColor`
- `shadingMode == 1 || shadingMode == 2`: Gouraud path (interpolated vertex color)
- `shadingMode == 3`: Phong path (interpolated normal/position + lighting per sample)

