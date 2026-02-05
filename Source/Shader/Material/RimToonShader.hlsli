#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_RimToon(MaterialInput materialInput)
{
    // --- 1. ライティング基礎値 ---
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    // --- 2. 階調化 ---
    float diffIntensity = length(lighting.diffuse);
    float toonDiff = lerp(0.15, 1.0,
        smoothstep(0.15, 0.20, diffIntensity) * 0.5 +
        smoothstep(0.50, 0.60, diffIntensity) * 0.5
    );

    // --- 3. ジグザグドット生成 ---
    float dotDensity = 15.0;
    float2 pixelPos = (materialInput.uv * materialInput.screenSize.xy) / dotDensity;
    float rowIndex = floor(pixelPos.y);
    pixelPos.x += frac(rowIndex * 0.5) > 0.0 ? 0.5 : 0.0;
    float2 dotUV = frac(pixelPos) - 0.5;
    float dist = length(dotUV);

    float shadowDotRadius = 0.3;
    float shadowDotPattern = smoothstep(shadowDotRadius + 0.1, shadowDotRadius - 0.1, dist);
    float patternMask = smoothstep(0.5, 0.3, toonDiff);
    
    // ドットの隙間の明るさ
    float shadowDotTone = lerp(1.0, 0.5 + shadowDotPattern * 0.5, patternMask);

    // --- 4. スペキュラ ---
    float specIntensity = length(lighting.specular);
    float toonSpec = smoothstep(0.45, 0.55, specIntensity);
    float3 finalSpecular = lighting.specular * toonSpec * (materialInput.baseColor.rgb + 0.5);

    // --- 【変更】常時リムライト計算 ---
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(materialInput.viewDir);
    
    // 1. 基本のフチ（Fresnel）
    float rimTerm = 1.0 - saturate(dot(N, V));

    // 2. リムの太さ調整（逆光判定を削除し、純粋な角度のみで計算）
    // 数値を大きくする(3.0など)と細く鋭くなり、小さくする(2.0など)と太く柔らかくなります
    float rimWeight = pow(rimTerm, 3.0);

    // 輪郭をクッキリさせる（しきい値でパキッと切る）
    float rimLogic = smoothstep(0.1, 0.15, rimWeight);

    // 色を決定（強さは 3.0倍 くらいが常時表示には丁度いいです）
    float3 rimColor = saturate(materialInput.baseColor.rgb + 0.8) * 3.0 * rimLogic;

    // --- 5. カラー合成 ---
    float3 baseColor = materialInput.baseColor.rgb;
    float3 shadowColor = baseColor * float3(0.8, 0.85, 0.95);
    float3 surfaceColor = lerp(shadowColor, baseColor, toonDiff) * shadowDotTone;

    // アンビエント底上げ
    float3 minAmbient = max(lighting.ambient, float3(0.4, 0.4, 0.4));

    // 合成
    float3 finalColor = (lighting.diffuse * toonDiff + minAmbient) * surfaceColor + finalSpecular;

    // リムライトを加算（常にフチが光る）
    finalColor += rimColor;

    return float4(saturate(finalColor), materialInput.baseColor.a);
}