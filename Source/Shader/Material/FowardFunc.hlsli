#ifndef FORWARD_FUNC_HLSLI
#define FORWARD_FUNC_HLSLI

#ifndef MATERIAL_DEFINE_HLSLI
#include "MaterialDefine.hlsli"
#endif

#ifndef COMMON_DEFINE_HLSLI
#include "../commonDefine.h"
#endif

#ifndef COMMON_HLSL
#include "../common.hlsl"
#endif

// ================= Texture =================
Texture2D BaseColorTex : register(t0);
SamplerState LinearSampler : register(s0);

// ================= Shadow =================
Texture2D ShadowMap : register(t7);
SamplerComparisonState ShadowSampler : register(s1);

// ================= Environment Map =================
Texture2D EnvironmentMap : register(t8);
SamplerState EnvSampler : register(s3);

// ============================================================================
// Forward : MaterialInput
// ============================================================================
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    // ===== World =====
    input.worldPos = In.WorldPosition.xyz;
    input.viewDir = normalize(CameraPosition.xyz - input.worldPos);

    // ===== Material =====
    input.baseColor = Material.BaseColor;
    // Diffuse Texture を使うか
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        input.baseColor *= BaseColorTex.Sample(LinearSampler, In.TexCoord);
    }

    input.normal = normalize(In.Normal.xyz);
    
    input.Metallic = Material.Metallic;
    input.Roughness = Material.Roughness;
    input.AO = Material.AO;
    input.Emissive = float4(Material.EmissiveColor, Material.EmissiveIntensity);

    // ===== IDs =====
    input.materialID = ShaderID;
    input.objectID = ObjectID;
    input.sceneID = SceneID;
    input.materialFlags = Material.MaterialFlags;
    
    BaseColorTex.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.Position.xy / float2(input.screenSize.x, input.screenSize.y);
    
    return input;
}

float SampleCascadePCF(float2 suvBase, float depth, float2 texelSize, float stepTexel, int radius);

float SampleShadowAtlasPCF(
    float2 uv,
    float depth,
    int tileIndex,
    ShadowPCFParams pcf)
{
    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;

    uint gx = tileIndex % grid;
    uint gy = tileIndex / grid;

    float2 tileMin = float2(gx, gy) * tile;
    float2 suvBase = tileMin + uv * tile;

    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    float2 texelSize = float2(1.0 / texW, 1.0 / texH);
    texelSize *= tile;

    int radius = max(pcf.KernelRadius, 0);
    return SampleCascadePCF(
        suvBase,
        depth,
        texelSize,
        pcf.StepTexel,
        radius
    );
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

    return SampleShadowAtlasPCF(uv, depth, lightIndex, pcf);
}

int SelectPointShadowFace(float3 direction)
{
    float3 absDir = abs(direction);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        return (direction.x >= 0.0) ? 0 : 1;
    }

    if (absDir.y >= absDir.z)
    {
        return (direction.y >= 0.0) ? 2 : 3;
    }

    return (direction.z >= 0.0) ? 4 : 5;
}

float ShadowFactorPoint(
    float3 worldPos,
    int firstLightIdx,
    int faceCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    if (firstLightIdx >= LIGHT_MAX_COUNT || faceCount <= 0)
        return 1.0;

    LIGHT firstFaceLight = Lights[firstLightIdx];
    float3 direction = worldPos - firstFaceLight.Position.xyz;
    int selectedFace = SelectPointShadowFace(direction);

    if (selectedFace >= faceCount)
        return 1.0;

    int faceLightIdx = firstLightIdx + selectedFace;
    if (faceLightIdx >= LIGHT_MAX_COUNT)
        return 1.0;

    LIGHT faceLight = Lights[faceLightIdx];
    if (!faceLight.Enable || !faceLight.CastShadow || faceLight.LightType != LIGHT_TYPE_POINT || faceLight.Dummy > -1)
        return 1.0;

    float4 sp = mul(float4(worldPos, 1.0), faceLight.LightView);
    sp = mul(sp, faceLight.LightProjection);

    if (sp.w <= 0.0)
        return 1.0;

    sp.xyz /= sp.w;

    float2 uv = sp.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;

    if (any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float depth = saturate(sp.z - faceLight.Param.w);
    return SampleShadowAtlasPCF(uv, depth, atlasOffset + selectedFace, pcf);
}

// =====================================================
// CSM PCF サンプリングヘルパー
// =====================================================
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
// CSM シャドウファクター (Cascade Blending 版)
// =====================================================
float ShadowFactorCascades(
    float3 worldPos,
    int firstLightIdx,
    int cascadeCount,
    int atlasOffset,
    ShadowPCFParams pcf)
{
    //--------------------------------------------------
    // 共通計算（ループ外に出せるものは事前計算）
    //--------------------------------------------------
    uint texW, texH;
    ShadowMap.GetDimensions(texW, texH);

    uint grid = (uint) ceil(sqrt((float) ShadowAtlasCount));
    float tile = 1.0 / grid;
    float2 texelSize = float2(1.0 / texW, 1.0 / texH) * tile;

    int radius = max(pcf.KernelRadius, 0);

    float finalShadow = 1.0;

    //--------------------------------------------------
    // カスケード評価ループ (フォールバック統合型)
    //--------------------------------------------------

    [loop]
    for (int c = 0; c < cascadeCount; c++)
    {
        int safeIdx = min(firstLightIdx + c, LIGHT_MAX_COUNT - 1);
        LIGHT cLight = Lights[safeIdx];

        float4 csp = mul(float4(worldPos, 1.0), cLight.LightView);
        csp = mul(csp, cLight.LightProjection);

        if (csp.w <= 0.0)
            continue;

        float3 ndc = csp.xyz / csp.w;
        float2 cuv = ndc.xy * 0.5 + 0.5;
        cuv.y = 1.0 - cuv.y;
        float cdepth = ndc.z;

        // 1. World座標からのUV範囲計算と深度チェック
        bool inUV = all(cuv >= 0.0) && all(cuv <= 1.0);

        // 範囲外なら次のカスケードをチェック (フォールバック)
        if (!inUV)
            continue;

        // 2. 範囲内なのでシャドウをサンプリング
        float bias = cLight.Param.w;
        float depth = saturate(cdepth - bias);

        int tileIndex = atlasOffset + c;
        uint gx = tileIndex % grid;
        uint gy = tileIndex / grid;
        float2 tileMin = float2(gx, gy) * tile;
        float2 suvBase = tileMin + cuv * tile;

        float shadow = SampleCascadePCF(
            suvBase,
            depth,
            texelSize,
            pcf.StepTexel,
            radius
        );
        // 3. 高層ビル等への対応 (本当に必要な状態かの確認)
        // 影が全く落ちていない(shadow >= 1.0)場合、手前のNear面で
        // オクルーダーがクリップされている可能性があるため、確定させずに
        // 次のカスケードへフォールバック(continue)する。
        if (shadow >= 1.0 && c < cascadeCount - 1)
        {
            continue;
        }
        
        //--------------------------------------------------
        // ご要望の処理: 遮蔽物のワールド座標復元と無効化判定
        //--------------------------------------------------
        if (shadow < 1.0 && c > 0)
        {
            // アトラス内のUVからピクセル座標を計算して生深度をフェッチ
            int3 loadCoord = int3((int) (suvBase.x * texW), (int) (suvBase.y * texH), 0);
            float rawDepth = ShadowMap.Load(loadCoord).r;

            float zScale = cLight.LightProjection[2][2];
            // Reversed-Z を考慮し、絶対値でNDC深度差分を計算
            float deltaZ_ndc = abs(cdepth - rawDepth);
                
                // Zスケールで割り戻すことで、View空間での実際の距離を算出
            float deltaZ_view = deltaZ_ndc / abs(zScale);

                // 遮蔽物はレシーバー(worldPos)より光源側にあるため、ライト方向の逆へ遡る
            float3 occluderPos = worldPos - cLight.Direction.xyz * deltaZ_view;

                // 前回のカスケード(c - 1)のビューに入っているか判定
            LIGHT prevLight = Lights[safeIdx - 1];
            
            float4 prevSp = mul(float4(occluderPos, 1.0), prevLight.LightView);
            prevSp = mul(prevSp, prevLight.LightProjection);
            
            LIGHT firstLight = Lights[min(firstLightIdx, LIGHT_MAX_COUNT - 1)];
            float4 firstSp = mul(float4(occluderPos, 1.0), firstLight.LightView);
            firstSp = mul(firstSp, firstLight.LightProjection);
            float3 firstNdc = firstSp.xyz / firstSp.w;

            if (prevSp.w > 0.0)
            {
                float3 prevNdc = prevSp.xyz / prevSp.w;
                float2 prevUv = prevNdc.xy * 0.5 + 0.5;
                prevUv.y = 1.0 - prevUv.y;

                if (all(prevUv > 0.0) && all(prevUv < 1.0) && firstNdc.z > 0.0 && prevNdc.z < 1.0)
                {
                    continue;
                }
            }
        }

        //--------------------------------------------------
        // ここに到達した場合は「影が落ちている」または「最後のカスケード」
        // なので影を確定させ、必要に応じてブレンドを行う
        //--------------------------------------------------
        finalShadow = min(finalShadow, shadow);
        
        if (0.0f >= finalShadow)
        {
            // 最適な影が確定したため、ループを抜ける
            break;
        }
    }

    return finalShadow;
}


#endif
