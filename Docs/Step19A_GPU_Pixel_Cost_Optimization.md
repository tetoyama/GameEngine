# Step 19-A GPU Pixel Cost Optimization

## Current execution pointer

Status: **highest priority / in progress**

The pass-level GPU timestamp foundation is implemented and the first runtime baseline identified Player Lighting as the dominant GPU scope.

Measured baseline on 2026-06-30:

```text
GPU Frame:          61.6301 ms
Player Lighting:    39.6063 ms
Player Post Effect: 17.5012 ms
Player Shadow:       2.1187 ms
Player GBuffer:      0.8786 ms
ImGui:               0.0512 ms
Unaccounted GPU:     1.4742 ms
```

Player Lighting accounts for approximately 64.3% of GPU Frame time. Lighting and Post Effect together account for approximately 92.7%.

The immediate procedure is maintained in:

- `Docs/Step19A2_Lighting_Bottleneck_Diagnosis.md`

Important design constraint:

- Do not unify material implementations with `switch(materialID)` or a dynamic if/else chain.
- That approach was previously tested and was slower because divergent material paths can serialize within a GPU wave.
- Keep material-specific deferred pixel shaders during diagnosis.
- Use only draw-uniform diagnostic controls for Shadow evaluation, PCF radius, Environment reflection, and maximum active light entries.

Immediate order:

1. Keep Compile Gate and rendering correctness green.
2. Measure Baseline.
3. Measure No Shadow while retaining Shadow Map generation.
4. Compare PCF 1x1 / 3x3 / 5x5 / material default.
5. Measure No Environment.
6. Compare maximum active light entries at 1 / 2 / 4 / 8 / all.
7. Select exactly one permanent Lighting optimization from the measured result.
8. Split and optimize the 17.5 ms Post Effect scope.
9. Continue fixed Render Scale and GBuffer bandwidth work.

Implemented diagnostic controls are available at `Project Settings > Lighting` and are runtime-only. They are not serialized to Scene or Project YAML.

---

## Background and retained plan

Player View or Window enlargement causes a major frame-rate reduction even when object count remains unchanged. This indicates that resolution-dependent GPU work is more significant than CPU Entity traversal or Draw Call generation.

The primary resolution-dependent candidates remain:

- GBuffer Multiple Render Target writes
- Deferred Lighting GBuffer reads
- Lighting-side Shadow filtering
- Full-screen Post Effects
- simultaneous Editor View and Player View rendering
- intermediate Render Target traffic
- Render Target format and total Bytes Per Pixel

The current GBuffer pixel shader outputs six color targets:

```text
Target 0: Albedo
Target 1: World Normal
Target 2: World Position
Target 3: Material
Target 4: Emissive
Target 5: uint4 Parameter
```

World Position may be reconstructed from depth. The uint4 Parameter target may be larger than required, especially for Player View. These format changes remain planned, but are not applied before the current 39.6 ms Lighting cost is decomposed.

## General measurement contract

- Do not interpret CPU Present duration as GPU execution time.
- Use asynchronous D3D11 Timestamp / Disjoint queries.
- Do not block on query results in the submitting frame.
- Record Player and Editor passes independently.
- Keep resolution, scene, camera, light configuration, and Post Effect configuration fixed during an A/B comparison.
- Warm up for at least 60 frames.
- Record at least 120 frames and retain average and P95.
- Change one performance variable per comparison.
- Preserve a default path that produces the pre-diagnostic visual result.

## Implemented pass timing scopes

- Player GBuffer
- Player Shadow
- Player Lighting
- Player Forward
- Player Post Effect
- Player Overlay
- Editor GBuffer
- Editor Shadow
- Editor Lighting
- Editor Forward
- Editor Post Effect
- Editor Overlay
- Editor PhysX Debug
- ImGui
- GPU Frame Total
- Accounted GPU
- Unaccounted GPU

## Lighting diagnosis interpretation

### Shadow sampling bound

Evidence:

- No Shadow substantially lowers Player Lighting.
- GPU cost scales with PCF radius.

Next candidates:

- standard quality: 3x3 PCF
- low quality: 1x1 PCF
- select the CSM cascade before filtering
- evaluate an adjacent cascade only near a split boundary
- reduce shadowed-light screen coverage
- static shadow caching and lower update frequency

### Light-loop bound

Evidence:

- limiting maximum active light entries lowers Player Lighting approximately in proportion to the entry count.

Next candidates:

- Tiled or Clustered light culling
- separate global Directional lights from local Point / Spot lights
- separate logical lights from expanded Shadow Atlas entries
- avoid iterating CSM and Point-face dummy entries as ordinary lights

### Environment-reflection bound

Evidence:

- No Environment substantially lowers Player Lighting.

Next candidates:

- replace the current eight runtime samples with a prefiltered environment map
- select mip level from roughness
- reduce to approximately one texture sample per shaded pixel

### Material-specific fullscreen draw bound

Evidence:

- Shadow, Environment, and light-count reductions do not explain the cost.
- Lighting cost increases with the number of material shader classes.

Do not return to a materialID switch. Investigate:

- material class written to stencil during GBuffer
- material-specific Lighting draw restricted by stencil before pixel-shader invocation
- a small fixed set of standard deferred material classes
- special materials moved to Forward or dedicated passes

Stencil use must first be checked against Editor Picking, existing depth/stencil ownership, and the required material-class count.

## Remaining GPU pixel-cost phases

### Resolution control

- Player and Editor fixed Render Scale
- independent internal render dimensions
- final bilinear upscale for the first implementation
- resize debounce
- display-to-internal Picking coordinate conversion

### GBuffer bandwidth budget

Record the actual format, Bytes Per Pixel, producer, consumers, and Player/Editor necessity for:

- Albedo
- Normal
- World Position
- Material
- Emissive
- Scene/Object/Shader/Flags
- Depth

### World Position removal

- add a shared depth-position reconstruction helper
- compare reconstructed position against the current GPosition target
- migrate Lighting first
- migrate SSAO / SSR / Post consumers individually
- remove the target only after all consumers are migrated

### Parameter target reduction

- separate Editor Picking from Player material classification
- avoid Player Scene/Object ID writes when not needed
- determine actual bit ranges before packing
- preserve explicit Invalid and Clear values

### Lower-resolution effects

Initial candidates:

- SSAO: half resolution
- SSR: half resolution, quarter at low quality
- Bloom: half-resolution mip chain
- volumetric and blur effects: half or quarter resolution

Use depth/normal-aware upsampling where simple bilinear filtering leaks across edges.

### Fullscreen pass fusion

Only fuse simple consecutive color transforms. Do not force neighborhood, ray-march, depth/normal, or history operations into one shader.

## Completion conditions

- The main source of the 39.6 ms Lighting cost is identified numerically.
- No materialID switch integration is used.
- Default diagnostic settings preserve the original result.
- Shadow, PCF, Environment, and active-light costs are measurable independently.
- One permanent Lighting optimization is selected from evidence.
- Post Effect receives its own internal scope breakdown after Lighting diagnosis.
- Internal resolution is separated from display size.
- GBuffer Bytes Per Pixel is reduced with individual before/after measurements.
- 720p / 1080p / 1440p results are recorded.
- Window enlargement causes a smaller frame-rate reduction than the baseline.
