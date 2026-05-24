# Close2GL CPU Mode Sequence Diagram

This diagram focuses on the frame path when `appearance.close2GlMode == true`.

```mermaid
sequenceDiagram
    autonumber
    participant UI as Camera/UI Controls
    participant Main as main.cpp Render Loop
    participant Raster as close_to_gl_rasterizer.cpp
    participant Frame as RasterFrame (CPU buffers)
    participant GL as OpenGL (Texture + Quad)

    UI->>Main: Update appearance/camera (drawMode, shadingMode, winding, pointSize)
    Main->>Main: Build model/view/projection matrices

    Main->>Raster: ensureRasterFrameSize(rasterFrame, w, h)
    Raster->>Frame: Resize color/depth, create/recreate textureId if needed
    Main->>Raster: clearRasterFrame(rasterFrame, clearColor)
    Raster->>Frame: Fill color + reset depth to 1.0

    loop For each triangle
        Main->>Main: Transform vertices to eye and clip space
        Main->>Main: rv[i].invW, rv[i].ndc, rv[i].screen=ndcToScreen(...)
        Main->>Raster: trianglePassesClipZ(rv[0].ndc, rv[1].ndc, rv[2].ndc)
        Raster-->>Main: pass/fail
        Main->>Raster: isFrontFacingInNdc(rv[0].ndc, rv[1].ndc, rv[2].ndc, frontFaceClockwise)
        Raster-->>Main: pass/fail

        alt Gouraud shading (mode 1 or 2)
            Main->>Raster: evaluatePhongLighting(...) per vertex
            Raster-->>Main: rv[i].gouraudColor
        end

        alt Draw points (drawMode 2)
            Main->>Raster: rasterizeVertexPoint(rv[0..2], shadingMode, ...)
            Raster->>Frame: shadeAndWritePixel(...) with depth test
        else Draw wireframe (drawMode 1)
            Main->>Raster: rasterizeWireTriangle(rv0, rv1, rv2, shadingMode, ...)
            Raster->>Frame: rasterizeWireEdge(...) x3 -> shadeAndWritePixel(...)
        else Draw solid (drawMode 0)
            Main->>Raster: rasterizeSolidTriangle(rv0, rv1, rv2, shadingMode, ...)
            Raster->>Frame: edgeFunction + interpolation + shadeAndWritePixel(...)
        end
    end

    Main->>GL: glBindTexture(rasterFrame.textureId)
    Main->>GL: glTexSubImage2D(..., rasterFrame.color.data())
    Main->>GL: Draw fullscreen quad (quadProgram)
    GL-->>Main: Present CPU-rasterized frame on window
```



## Notes

- `main.cpp` orchestrates the frame and prepares per-vertex inputs.
- `close_to_gl_rasterizer.cpp` handles culling helpers, interpolation, shading, and per-pixel depth/color writes.
- Final presentation is still done through OpenGL by uploading the CPU color buffer to a texture and drawing a fullscreen quad.

