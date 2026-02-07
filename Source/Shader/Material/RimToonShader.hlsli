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

    // 光の強さ (0.1:シャドウマップ影 ～ 1.0:直射日光)
    float diffIntensity = length(lighting.diffuse);

    // --- 2. ドット座標計算（45度回転） ---
    float dotDensity = 10.0; // ドットが小さい場合はここを下げてください（例: 15.0 -> 10.0）
    
    float c = 0.7071;
    float s = 0.7071;
    float2x2 rotMat = float2x2(c, -s, s, c);
    float2 screenUV = materialInput.uv * materialInput.screenSize.xy;
    float2 rotatedPos = mul(screenUV, rotMat);
    float2 pixelPos = rotatedPos / dotDensity;
    float rowIndex = floor(pixelPos.y);
    pixelPos.x += frac(rowIndex * 0.5) > 0.0 ? 0.5 : 0.0;
    float2 dotUV = frac(pixelPos) - 0.5;
    
    // 中心からの距離 (円形ドット用)
    float dist = length(dotUV);

    // --- 【重要】ドットサイズ（光の半径）の計算 ---
    
    // ★ポイント1：シャドウマップ対策（足切り）
    // diffIntensity が 0.2 以下の時は、強制的に半径 0.0 にする。
    // これにより、シャドウマップ(0.1)の部分は「光のドット」が一切描かれない＝完全な影色になる。
    
    // ★ポイント2：ドットの巨大化
    // 明るくなるにつれて半径を大きくし、最終的に 0.8 以上にして隙間をなくす（ベタ塗りの日向にする）。
    
    // 0.15(暗) -> 半径0.0 (消滅)
    // 0.60(明) -> 半径0.9 (巨大化して隣と融合)
    float dotRadius = smoothstep(0.15, 0.60, diffIntensity) * 0.9;

    // ドットを描画（dist が radius より小さい場所が「光」になる）
    // smoothstepを使って少し輪郭を滑らかにする
    float lightDot = 1.0 - smoothstep(dotRadius - 0.05, dotRadius + 0.05, dist);

    // --- 3. 影色と表面色の合成 ---
    float3 baseColor = materialInput.baseColor.rgb;
    
    // リッチな影色（彩度高め）
    float3 shadowBase = pow(baseColor, 1.5) * float3(0.9, 0.9, 1.1);
    float3 shadowColor = lerp(baseColor * 0.5, shadowBase, 0.7);

    // ★合成ロジック：ベースは「影色」。そこに「光ドット」を上書きする。
    // これにより、ドットがない場所（深い影）は自然とshadowColorだけが残る。
    float3 surfaceColor = lerp(shadowColor, baseColor, lightDot);

    // --- 4. スペキュラ ---
    float specIntensity = length(lighting.specular);
    float toonSpec = smoothstep(0.45, 0.55, specIntensity);
    float3 finalSpecular = lighting.specular * toonSpec * (materialInput.baseColor.rgb + 0.5);

    // --- 5. リムライト ---
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(materialInput.viewDir);
    float rimTerm = 1.0 - saturate(dot(N, V));
    float rimWeight = pow(rimTerm, 3.0);
    float rimLogic = smoothstep(0.1, 0.15, rimWeight);
    float3 rimColor = saturate(materialInput.baseColor.rgb + 0.8) * 3.0 * rimLogic;

    // --- 6. 最終合成 ---
    // アンビエント底上げ
    float3 minAmbient = max(lighting.ambient, float3(0.4, 0.4, 0.4));

    // ライティング計算
    // ドット(lightDot)自体が明るさ情報を持っているので、diffIntensityを直接乗算する必要はないが、
    // 全体の陰影を馴染ませるために ambient と混ぜたものを乗せる
    float3 finalColor = (minAmbient + 0.5 * lightDot) * surfaceColor + finalSpecular;
    
    finalColor += rimColor;

    return float4(saturate(finalColor), materialInput.baseColor.a);
}