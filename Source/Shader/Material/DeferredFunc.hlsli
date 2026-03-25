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
// ライティングパスで参照する GBuffer 群
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
// GBuffer からマテリアル計算用の入力を復元する
MaterialInput GetMaterialInput(PS_IN In)
{
    MaterialInput input;
    input.uv = In.TexCoord;

    input.baseColor =
        float4(GAlbedo.Sample(PointSampler, input.uv).rgb, 1);

    input.normal =
        normalize(GNormal.Sample(PointSampler, input.uv).rgb * 2.0 - 1.0);

    uint textureWidth, textureHeight;
    GPosition.GetDimensions(textureWidth, textureHeight);
    int2 pixelCoord = int2(In.TexCoord.x * textureWidth, In.TexCoord.y * textureHeight);

    float3 worldPos = GPosition.Load(int3(pixelCoord, 0)).xyz;
    
    input.worldPos = worldPos;

    input.viewDir = normalize(CameraPosition.xyz - worldPos);

    float4 mat = GMaterial.Sample(PointSampler, input.uv);
    input.Metallic = mat.r;
    input.Roughness = mat.g;
    input.AO = mat.b;
    input.Emissive = GEmissive.Sample(PointSampler, input.uv);

    uint4 param = GParam.Load(int3(pixelCoord, 0));
    input.sceneID = param.x;
    input.objectID = param.y;
    input.materialID = param.z;
    input.materialFlags = param.w;

    GAlbedo.GetDimensions(input.screenSize.x, input.screenSize.y);
    input.screenUV = In.TexCoord;

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


#endif
