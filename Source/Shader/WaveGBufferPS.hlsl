// =============================================================
// WaveGBufferPS.hlsl
//
// 水面専用 GBuffer ピクセルシェーダ
// Gerstner波で計算された頂点法線を基に、タイリング法線アニメーションを重ねる。
// 高い金属度・低いラフネスを設定することで、SSRやスペキュラによる
// 反射表現に対応したリアルな水面を実現する。
//
// Parameter (CbPerObject.Parameter):
//   x : NormalIntensity   法線アニメーション強度 (0 = 頂点法線のみ, 1 = 強いアニメ)
//   y : WaterRoughness    水面ラフネス           (推奨: 0.05)
//   z : WaterMetallic     水面金属度             (推奨: 0.9)
//   w : Time              経過時間 (WaveSystem が更新)
// =============================================================

#include "common.hlsl"
#include "commonDefine.h"

struct GBUFFER_OUT
{
    float4 OutAlbedo   : SV_Target0;
    float4 OutNormal   : SV_Target1;
    float4 OutPosition : SV_Target2;
    float4 OutMaterial : SV_Target3;
    float4 OutEmissive : SV_Target4;
    uint4  OutParam    : SV_Target5;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

GBUFFER_OUT main(PS_IN In)
{
    GBUFFER_OUT Out;

    // ---- パラメータ ----
    float normalIntensity = Parameter.x;
    float roughness       = saturate(Parameter.y);
    float metallic        = saturate(Parameter.z);
    float time            = Parameter.w;

    // ---- アルベド (水色ベース) ----
    float4 baseColor = Material.BaseColor;
    if ((Material.MaterialFlags & MATERIAL_FLAG_USE_DIFFUSE_TEXTURE) != 0)
    {
        baseColor *= DiffuseTexture.Sample(LinearSampler, In.TexCoord);
    }
    Out.OutAlbedo = baseColor;

    // ---- 法線アニメーション ----
    // 頂点法線 (Gerstner波で計算済み)
    float3 N = normalize(In.Normal.xyz);

    // ---- タンジェントが有効かどうかを正規化前に確認 ----
    float lenT = length(In.Tangent.xyz);
    float3 T;
    if (lenT < 0.001f)
    {
        // タンジェントが無効な場合は法線から再計算
        float3 up = abs(N.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        T = normalize(cross(up, N));
    }
    else
    {
        T = normalize(In.Tangent.xyz);
    }
    float3 B = normalize(cross(N, T));

    // 2層の法線アニメーション (スクロールUVで波紋を表現)
    float2 uv   = In.TexCoord;
    float2 uv1  = uv * 3.0f + float2( time * 0.04f,  time * 0.02f);
    float2 uv2  = uv * 2.0f + float2(-time * 0.03f,  time * 0.05f);

    float3 detail1;
    detail1.x = sin(uv1.x * 6.2832f) * 0.5f;
    detail1.y = 1.0f;
    detail1.z = cos(uv1.y * 6.2832f) * 0.5f;
    detail1 = normalize(detail1);

    float3 detail2;
    detail2.x = cos(uv2.x * 6.2832f) * 0.5f;
    detail2.y = 1.0f;
    detail2.z = sin(uv2.y * 6.2832f + 1.0f) * 0.5f;
    detail2 = normalize(detail2);

    // 2つの法線アニメを平均して TBN でワールド空間へ変換
    float3 detailNormal = normalize(detail1 + detail2);
    float3 worldDetail  = normalize(
        T * detailNormal.x + B * detailNormal.z + N * detailNormal.y
    );

    // 頂点法線とブレンド
    float3 finalNormal = normalize(lerp(N, worldDetail, saturate(normalIntensity)));
    Out.OutNormal = float4(finalNormal * 0.5f + 0.5f, 1.0f);

    // ---- ワールド座標 ----
    Out.OutPosition.xyz = In.WorldPosition.xyz;
    Out.OutPosition.w   = 1.0f;

    // ---- マテリアルパラメータ ----
    // (x: Metallic, y: Roughness, z: AO, w: 0)
    Out.OutMaterial = float4(metallic, roughness, 1.0f, 0.0f);

    // ---- エミッシブ ----
    Out.OutEmissive = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // ---- Param ----
    Out.OutParam = uint4(ObjectID, SceneID, ShaderID, 0);

    return Out;
}
