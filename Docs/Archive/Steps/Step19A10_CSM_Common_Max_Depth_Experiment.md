# Configurable Directional CSM Light

## Purpose

Move CSM quality and performance controls from compile-time experiments into each `LightComponent` whose type is `DirectionalCSM`.

The shader and GPU packing capacity remains four cascades. Each CSM light selects how many cascade entries `ShadowMapPass` generates at runtime.

## Inspector workflow

The normal Inspector presents the controls in the same order as a conventional Unity shadow workflow:

1. `Cast Shadows`
2. Shadow `Distance`
3. `Cascades`
4. Automatic or Manual `Distribution`
5. Color-coded Cascade Split preview
6. `Near Detail`
7. `Bias` and `Normal Bias`

Implementation-specific parameters are kept under `Advanced`:

- automatic split distribution / PSSM Lambda
- XY detail falloff exponent
- shadow bias storage mode
- reset to the tested default configuration

Manual split values are edited as percentages. Hovering the split preview shows each cascade's world-distance interval and effective near-detail multiplier.

## Tested default

The runtime default is intentionally identical to the final compile-time Macro configuration used before the Inspector conversion:

```text
Cascade Count:            3
Shadow Distance:          Camera Far Clip
Split Mode:               Practical PSSM
PSSM Lambda:              0.85
Near XY Footprint Scale:  0.50 (Near Detail 2x)
XY Distribution Exponent: 1.50
Common Z:                 disabled
Primary-only sampling:    disabled
Legacy cascade fallback:  enabled
```

`Reset to Tested Defaults` restores this configuration. The final cascade always resolves to an XY scale of `1.0`, preserving the original outer coverage.

Scenes saved before the `CSM` map existed receive these defaults when loaded. Saving the scene writes the explicit settings.

## Runtime behavior

`ShadowMapPass` performs the following for the selected Directional CSM light:

1. Clamp the selected cascade count to 1–4.
2. Resolve CSM far distance from `Distance` and the active camera.
3. Build either PSSM or manual cascade split depths.
4. Calculate the receiver-fitted cascade sphere.
5. Apply the configured XY footprint distribution.
6. Emit only the selected number of packed `LIGHT` entries.

Reducing cascade count removes a full per-cascade culling, packet traversal, static-batch submission and shadow render.

Reducing shadow distance can improve spatial density and reduce the number of relevant casters, while leaving the camera far plane unchanged.

## YAML serialization

Settings are stored under the Light Component's `CSM` map:

```yaml
CSM:
  CascadeCount: 3
  ShadowDistance: 0.0
  SplitMode: 0
  SplitLambda: 0.85
  ManualSplitRatios: [0.1, 0.3, 0.6, 1.0]
  XyMinScale: 0.5
  XyScaleExponent: 1.5
```

## Performance validation

Use the same `_scene.scene` and camera pose. Compare at least 120 stable samples for:

```text
Cascades: 4 vs 3
Distance: Camera Far vs a shorter explicit value
```

Record:

- GPU Frame Avg / P95
- Player Shadow Avg / P95
- Player Lighting Avg / P95
- Draw CPU
- Render Schedule CPU
- far-shadow coverage
- cascade-boundary visibility

Acne and receiver-bias tuning remain separate from the performance comparison.
