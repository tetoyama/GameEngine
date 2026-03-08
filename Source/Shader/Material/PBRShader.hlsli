#ifndef MATERIAL_FUNC_HLSLI
#include "MaterialFunc.hlsli"
#endif

// ------------------------------------------------------------
// 方向ベクトル → Equirectangular UV 変換 (最適化版)
// ------------------------------------------------------------
float2 DirToSkyUV(float3 dir)
{
    float phi = atan2(dir.x, dir.z);
    float theta = asin(clamp(dir.y, -1.0f, 1.0f));

    // 1.0 / (2.0 * PI) = 0.1591549, 1.0 / PI = 0.3183098
    return float2(phi * 0.1591549f + 0.5f, 0.5f - theta * 0.3183098f);
}

// ------------------------------------------------------------
// ワールド座標ベースの疑似乱数
// sv_position の代わりに worldPos を使用してジッタを生成
// ------------------------------------------------------------
float GetNoiseFromWorldPos(float3 worldPos)
{
    // ワールド座標の各成分を組み合わせて二次元的なシードを作る
    float2 seed = worldPos.xy + worldPos.z;
    return frac(52.9829189f * frac(dot(seed, float2(0.06711056f, 0.00583715f))));
}

// ------------------------------------------------------------
// ラフネスを考慮したフレネル・シュリック近似
// ------------------------------------------------------------
float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((float3) (1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

// ------------------------------------------------------------
// PBR Material (WorldPos Jitter Vogel Disk Sampling)
// ------------------------------------------------------------
float4 ShadeMaterial_PBR(MaterialInput materialInput)
{
    float3 N = normalize(materialInput.normal);
    float3 V = normalize(CameraPosition.xyz - materialInput.worldPos);
    float dotNV = saturate(dot(N, V));
    
    float roughness = saturate(materialInput.Roughness);
    float metallic = saturate(materialInput.Metallic);
    float3 baseColor = materialInput.baseColor.rgb;

    // 【修正】メタリックが 0 の時の反射率(F0)を調整
    // 物理的に正しくしたいなら 0.04 固定ですが、
    // 反射を消したいなら 0.04 * metallic にするか、別途反射強度パラメータを用意します。
    float3 F0 = baseColor * metallic;
    
    
    // ダイレクトライト
    ShadowPCFParams pcf = { 2, 1 };
    LightingResult lighting = ComputeLightingFromMaterialInput(materialInput, pcf);

    float3 envSpecular = 0;
    float3 F = FresnelSchlickRoughness(dotNV, F0, roughness);

    if (materialInput.materialFlags & MATERIAL_FLAG_USE_ENVIRONMENT_MAP)
    {
        float3 R = reflect(-V, N);
        float3 up = abs(R.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
        float3 T = normalize(cross(up, R));
        float3 B = cross(R, T);

        float spread = roughness * roughness;
        float noise = GetNoiseFromWorldPos(materialInput.worldPos);
        float angle = noise * 2.0f * PI;
        float s, c;
        sincos(angle, s, c);

        float3 envAccum = 0;
        const int SAMPLE_COUNT = 8;

        [unroll]
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
            float r = sqrt(i + 0.5f) / sqrt((float) SAMPLE_COUNT);
            float theta = i * 2.3999632f;
            float2 p = float2(cos(theta), sin(theta)) * r * spread;
            float2 rotatedP = float2(p.x * c - p.y * s, p.x * s + p.y * c);

            float3 sampleDir = normalize(R + T * rotatedP.x + B * rotatedP.y);
            envAccum += EnvironmentMap.Sample(EnvSampler, DirToSkyUV(sampleDir)).rgb;
        }

        // 【調整】環境マップ自体の強度をメタリックでさらに絞ることも可能
        // envSpecular = (envAccum / (float)SAMPLE_COUNT) * F * metallic; // 完全に消したい場合
        envSpecular = (envAccum / (float) SAMPLE_COUNT) * F;
    }

    // --- エネルギー保存則 ---
    // 反射（kS）として扱われる分をフレネル F から取得
    float3 kS = F;
    // 拡散反射（kD）は「1.0 - 反射分」。メタリックであるほど拡散光は消失する。
    float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);

    // 最終合成
    float3 diffuse = saturate(lighting.diffuse + lighting.ambient) * baseColor * kD;
    float3 litColor = diffuse + lighting.specular + envSpecular;

    float3 emissive = materialInput.Emissive.rgb * materialInput.Emissive.a;

    return float4(saturate(litColor) + emissive, materialInput.baseColor.a);
}