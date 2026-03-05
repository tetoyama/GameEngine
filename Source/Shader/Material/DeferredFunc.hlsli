#ifndef DEFERRED_FUNC_HLSLI
#define DEFERRED_FUNC_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI
#include "MaterialDefine.hlsli"
#endif

#ifndef COMMON_DEFINE_HLSLI
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

// ================= GBuffer =================
Texture2D GAlbedo : register(t0);
Texture2D GNormal : register(t1);
Texture2D GPosition : register(t2);
Texture2D GMaterial : register(t3);
Texture2D GEmissive: register(t4);
Texture2D<uint4> GParam : register(t5);
SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s2);

// ================= Shadow =================
Texture2D ShadowMap : register(t6);
SamplerComparisonState ShadowSampler : register(s1);

// ================= Environment Map =================
Texture2D EnvironmentMap : register(t7);
SamplerState EnvSampler : register(s3);

// ================= Implement =================
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor =
        float4(GAlbedo.Sample(PointSampler, input.uv).rgb, 1);

    input.normal =
        normalize(GNormal.Sample(PointSampler, input.uv).rgb * 2.0 - 1.0);

    uint w, h;
    GPosition.GetDimensions(w, h);
    int2 pixelCoord = int2(In.TexCoord.x * w, In.TexCoord.y * h);

    float3 worldPos = GPosition.Load(int3(pixelCoord, 0)).xyz;
    
    input.worldPos = worldPos;

    input.viewDir = normalize(CameraPosition.xyz - worldPos);

    float4 mat = GMaterial.Sample(PointSampler, input.uv);
    input.Metallic = mat.r;
    input.Roughness = mat.g;
    input.AO = mat.b;
    input.Emissive = GEmissive.Sample(PointSampler, input.uv);

    uint4 param = GParam.Load(int3(pixelCoord, 0));
    input.objectID = param.x;
    input.sceneID = param.y;
    input.materialID = param.z;
    input.materialFlags = param.w;

    GAlbedo.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.TexCoord;

    return input;
}



// =====================================================
// CSM PCF サンプリングヘルパー
// =====================================================
// suvBase   : アトラス内タイル起点 UV
// depth     : 比較深度
// texelSize : タイルサイズ込みのテクセルサイズ
// stepTexel : PCF ステップ倍率 (pcf.StepTexel)
// radius    : PCF カーネル半径
float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius)
{
    float shadow = 0.0;
    int count = 0;
    [loop]
    for (int sy = -radius; sy <= radius; sy++)
    {
        [loop]
        for (int sx = -radius; sx <= radius; sx++)
        {
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + float2(sx, sy) * texelSize * stepTexel,
                depth);
            count++;
        }
    }
    return shadow / max(count, 1);
}


// =====================================================
// CSM (Cascaded Shadow Maps) シャドウファクター
// =====================================================
float ShadowFactorCSM(
    float3 worldPos,
    LIGHT  light,
    ShadowPCFParams pcf)
{
    // ---- カスケード選択 + ライトスペース変換 (投影UV判定) ----
    // スプリット深度に依存せず、各カスケードの実際の投影 UV [0,1] 内に収まるかを
    // 近→遠の順に検査し、最初にカバーされる（最も精細な）カスケードを選択する。
    // スプリット深度は C++ 側でのカスケード錐台計算にのみ使用し、
    // シェーダ側の選択は行列投影ベースで行うことで精度を最大化する。
    // ※ CSM は正射影なので csp.w は常に 1 だが、念のため w <= 0 は skip する。

    // デフォルトとして最遠カスケードで sp を事前計算 (どのカスケードもカバーしない場合のフォールバック)
    int cascade = DIRECTIONAL_CSM_CASCADE_COUNT - 1;
    float4 sp = mul(float4(worldPos, 1.0), CsmViews[cascade]);
    sp = mul(sp, CsmProjections[cascade]);

    // 近→遠の順で走査し、UV 内に収まる最初（最も精細）のカスケードを使う。
    // 遠→近にすると「最も粗い」カスケードが選ばれてしまい精度が下がるため、
    // 精度最大化のために近→遠の方向で break を使う設計とする。
    [loop]
    for (int c = 0; c < DIRECTIONAL_CSM_CASCADE_COUNT - 1; c++)
    {
        float4 csp = mul(float4(worldPos, 1.0), CsmViews[c]);
        csp = mul(csp, CsmProjections[c]);
        if (csp.w <= 0.0) continue; // w <= 0 はゼロ除算防止 (正射影では通常 w=1)
        float2 cuv = csp.xy / csp.w * 0.5 + 0.5;
        cuv.y = 1.0 - cuv.y;
        if (all(cuv >= 0.0) && all(cuv <= 1.0))
        {
            cascade = c;
            sp = csp;
            break;
        }
    }

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    // ---- シャドウバイアス (シャドウアクネ対策) ----
    // Param.w で調整可能: 値を大きくするとアクネが減り、小さくするとピーターパン現象が減る
    float bias = light.Param.w;
    float depth = saturate(sp.z - bias);

    // ---- アトラスタイル計算 ----
    int tileIndex = CsmAtlasOffset + cascade;
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    // ---- テクセルサイズ ----
    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;
    texelSize *= tile;

    // ---- PCF ----
    int radius = max(pcf.KernelRadius, 0);
    float shadow = SampleCascadePCF(suvBase, depth, texelSize, pcf.StepTexel, radius);

    // ---- カスケードフォールバック ----
    // 最精細カスケードが「影なし (shadow=1.0)」を返した場合、次のカスケードで遠方の
    // shadow caster による影を検証する。
    //
    // 次のカスケードで遮蔽物が見つかった場合、その遮蔽物のライトビュー Z を最精細
    // カスケードの NDC 空間に変換し、最精細カスケードの Z 範囲 [0,1] 内かを確認する。
    //   ・範囲内  → 最精細カスケードで捕捉可能なはず → 「影なし」の判定を優先して影を無視
    //   ・範囲外  → 最精細カスケードでは描画不可の遮蔽物 → 次のカスケードの影を採用
    [branch]
    if (shadow >= 1.0 && cascade < DIRECTIONAL_CSM_CASCADE_COUNT - 1)
    {
        int nextCascade = cascade + 1;
        float4 nsp = mul(float4(worldPos, 1.0), CsmViews[nextCascade]);
        nsp = mul(nsp, CsmProjections[nextCascade]);
        if (nsp.w > 0.0)
        {
            nsp.xyz /= nsp.w;
            float2 nuv = nsp.xy * 0.5 + 0.5;
            nuv.y = 1.0 - nuv.y;
            if (all(nuv >= 0.0) && all(nuv <= 1.0))
            {
                float ndepth = saturate(nsp.z - bias);
                int nTileIndex = CsmAtlasOffset + nextCascade;
                uint ngx = nTileIndex % grid;
                uint ngy = nTileIndex / grid;
                float2 nsuvBase = float2(ngx, ngy) * tile + nuv * tile;

                // 次のカスケードのシャドウマップから遮蔽物の生深度を読み取る
                float occRawDepth = ShadowMap.SampleLevel(LinearSampler, nsuvBase, 0).r;

                // 正射影: ndc_z = viewZ * P._33 + P._43  →  viewZ = (ndc_z - P._43) / P._33
                // 遮蔽物のライトビュー Z を次のカスケードの行列から求め、
                // 最精細カスケードの NDC Z に変換して描画範囲を確認する。
                float p1_33 = CsmProjections[nextCascade]._33;
                [branch]
                if (abs(p1_33) > 1e-6)
                {
                    float occViewZ = (occRawDepth - CsmProjections[nextCascade]._43) / p1_33;
                    float occNdcInFine = occViewZ * CsmProjections[cascade]._33 + CsmProjections[cascade]._43;

                    if (occNdcInFine < 0.0 || occNdcInFine > 1.0)
                    {
                        shadow = min(shadow, SampleCascadePCF(nsuvBase, ndepth, texelSize, pcf.StepTexel, radius));
                    }
                }
            }
        }
    }

    return shadow;
}


float ShadowFactor(
    float3 worldPos,
    LIGHT light,
    int lightIndex,
    ShadowPCFParams pcf)
{
    // ---- Light Space ----
    float4 sp = mul(float4(worldPos, 1.0), light.LightView);
    sp = mul(sp, light.LightProjection);

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    // ---- シャドウバイアス (シャドウアクネ対策) ----
    // Param.w で調整可能: 値を大きくするとアクネが減り、小さくするとピーターパン現象が減る
    float bias = light.Param.w;
    float depth = saturate(sp.z - bias);

    // ---- Atlas ----
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = lightIndex % grid;
    uint gy = lightIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    // ---- テクセルサイズ（1ピクセル）----
    float2 texelSize;
    ShadowMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = 1.0 / texelSize;

    texelSize *= tile; // アトラス対応

    // ---- PCF ----
    float shadow = 0.0;
    int radius = max(pcf.KernelRadius, 0);
    int count = 0;

    [loop]
    for (int y = -radius; y <= radius; y++)
    {
        [loop]
        for (int x = -radius; x <= radius; x++)
        {
            float2 offset =
                float2(x, y) * texelSize * pcf.StepTexel;

            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                suvBase + offset,
                depth);

            count++;
        }
    }

    return shadow / max(count, 1);
}


#endif
