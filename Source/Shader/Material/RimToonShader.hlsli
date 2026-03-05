#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_RimToon(MaterialInput materialInput)
{
    // --- 1. ライティング基礎 ---
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;
    LightingResult lighting =
        ComputeLightingFromMaterialInput(materialInput, pcf);

    float diffIntensity = length(lighting.diffuse);

    // ★ 光量を4諧調化
    float diffStep = Quantize4(diffIntensity);

    // --- 2. ドット（そのまま） ---
    float dotDensity = 5.0;

    float c = 0.7071;
    float s = 0.7071;
    float2x2 rotMat = float2x2(c, -s, s, c);

    float2 screenUV =
        materialInput.screenUV * materialInput.screenSize.xy;
    float2 rotatedPos = mul(screenUV, rotMat);
    float2 pixelPos = rotatedPos / dotDensity;

    float rowIndex = floor(pixelPos.y);
    pixelPos.x += frac(rowIndex * 0.5) > 0.0 ? 0.5 : 0.0;

    float2 dotUV = frac(pixelPos) - 0.5;
    float dist = length(dotUV);

    float dotRadius =
        smoothstep(0, 1.5, diffIntensity) * 0.9;

    float lightDot =
        1.0 - smoothstep(dotRadius - 0.05,
                          dotRadius + 0.05,
                          dist);

    // --- 3. 色合成（★ここが本命） ---
    float3 baseColor = materialInput.baseColor.rgb;

    float3 shadowBase =
        pow(baseColor, 1.5) * float3(0.9, 0.9, 1.1);
    float3 shadowColor =
        lerp(baseColor * 0.5, shadowBase, 0.7);

    // ★ ドット × 4諧調光量
    float lightFactor = lightDot * diffStep;

    float3 surfaceColor =
        lerp(shadowColor, baseColor, lightFactor);

    // --- 4. スペキュラ ---
    float specIntensity = length(lighting.specular);
    float toonSpec = smoothstep(0.45, 0.55, specIntensity);
    float3 finalSpecular =
        lighting.specular * toonSpec * (baseColor + 0.5);

    // --- 5. リム ---
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(materialInput.viewDir);

    float rimTerm = 1.0 - saturate(dot(N, V));
    float rimWeight = pow(rimTerm, 3.0);
    float rimLogic = smoothstep(0.1, 0.15, rimWeight);

    // リム色: ToonShader (0.8) より控えめなオフセット (0.1) でリムを暗めに表現
    float3 rimColor =
        saturate(baseColor + 0.1) * 3.0 * rimLogic;

    // --- 6. 最終合成 ---
    float3 minAmbient =
        max(lighting.ambient, float3(0.4, 0.4, 0.4));

    float3 finalColor =
        (minAmbient + 0.5 * lightDot) * surfaceColor
        + finalSpecular
        + rimColor;

    // Emissive contribution (HDR: not clamped, drives bloom)
    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;

    return float4(saturate(finalColor) + emissive,
                  materialInput.baseColor.a);
}
