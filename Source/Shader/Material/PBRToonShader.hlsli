#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

float4 ShadeMaterial_PBRToon(MaterialInput materialInput)
{
    ShadowPCFParams pcf;
    pcf.KernelRadius = 2;
    pcf.StepTexel = 1;
    
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    // --- 1. Diffuseの多段階調 (境界にわずかな滑らかさ) ---
    float diffIntensity = length(lighting.diffuse);
    float toonDiff = 0.15; // 最低の影の濃さ
    toonDiff += smoothstep(0.20, 0.22, diffIntensity) * 0.35; // 影 -> 中間
    toonDiff += smoothstep(0.55, 0.57, diffIntensity) * 0.50; // 中間 -> 日向

    // --- 2. リムライト (輪郭の光) ---
    // 視線方向と法線の角度から、縁に近いほど強くなる係数を作る
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(materialInput.viewDir);
    float rim = 1.0 - saturate(dot(N, V));
    
    // powでリムの幅を鋭くし、smoothstepでパキッとさせる
    rim = pow(rim, 4.0);
    float3 rimLight = smoothstep(0.48, 0.52, rim) * float3(1, 1, 1) * 0.6;
    
    // 【こだわり】影側にはリムを出さないように NdotL でマスクする（お好みで）
    // float NdotL = saturate(dot(N, normalize(-Lights[0].Direction.xyz)));
    // rimLight *= NdotL; 

    // --- 3. スペキュラ ---
    float specIntensity = length(lighting.specular);
    float toonSpec = smoothstep(0.48, 0.52, specIntensity);

    // --- 4. カラー合成 ---
    float3 baseColor = materialInput.baseColor.rgb;
    float3 shadowColor = baseColor * float3(0.6, 0.6, 0.8);
    float3 surfaceColor = lerp(shadowColor, baseColor, toonDiff);

    // 最終カラー計算
    float3 finalColor = (lighting.diffuse * toonDiff + lighting.ambient) * surfaceColor
                        + (lighting.specular * toonSpec);

    return float4(saturate(finalColor), materialInput.baseColor.a);
}