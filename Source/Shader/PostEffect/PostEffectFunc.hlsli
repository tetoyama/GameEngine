#include "../common.hlsl"

//======================================================
// ポストエフェクト用 GBuffer アクセスヘルパー
//
// 使い方:
//   1. このファイルをインクルードする
//   2. シーンテクスチャを t0 に宣言する
//      Texture2D g_Texture : register(t0);
//   3. GBuffer 取得関数 (GetAlbedo, GetWorldNormal, ...) を呼び出す
//      ※ g_SamplerState はこのファイルで定義済みのため再宣言不要
//======================================================

// GBuffer テクスチャ — C++ 側で PostEffectGBufferSlot_* に対応して自動バインドされる
Texture2D            GAlbedo   : register(t8);   // アルベド (BaseColor)
Texture2D            GNormal   : register(t9);   // ワールド法線 ([0,1] エンコード済み)
Texture2D            GPosition : register(t10);  // ワールド座標
Texture2D            GMaterial : register(t11);  // マテリアル (x:Metallic, y:Roughness, z:AO)
Texture2D            GEmissive : register(t12);  // エミッシブ
Texture2D<uint4>     GParam    : register(t13);  // per-object 整数パラメータ
Texture2D<float>     GDepth    : register(t14);  // 深度バッファ (NDC 深度 [0,1])

// サンプラー — 全ポストエフェクトで共通 (s0)
SamplerState g_SamplerState : register(s0);

//======================================================
// G-Buffer 取得関数群
//======================================================

// アルベド取得
float4 GetAlbedo(float2 uv)
{
    return GAlbedo.Sample(g_SamplerState, uv);
}

// ワールド法線取得 — GBuffer は [0,1] にエンコードされているので [-1,1] に変換
float3 GetWorldNormal(float2 uv)
{
    return GNormal.Sample(g_SamplerState, uv).xyz * 2.0 - 1.0;
}

// ワールド座標取得
float3 GetWorldPosition(float2 uv)
{
    return GPosition.Sample(g_SamplerState, uv).xyz;
}

// マテリアルパラメータ取得 (x: Metallic, y: Roughness, z: AO, w: 0 (未使用))
float4 GetMaterial(float2 uv)
{
    return GMaterial.Sample(g_SamplerState, uv);
}

// エミッシブ取得
float4 GetEmissive(float2 uv)
{
    return GEmissive.Sample(g_SamplerState, uv);
}

// per-object 整数パラメータ取得 (x: ObjectID, y: SceneID, z: ShaderID, w: 0 (未使用))
uint4 GetObjParam(int2 pixelCoord)
{
    return GParam.Load(int3(pixelCoord, 0));
}

// 深度取得 — NDC 深度 [0,1] をそのまま返す
float GetDepth(float2 uv)
{
    return GDepth.Sample(g_SamplerState, uv).r;
}

// 線形 view-space 深度取得 (カメラから物体までの距離, 単位: ワールド空間)
// 前提: DirectXMath XMMatrixPerspectiveFovLH で生成された射影行列
// 公式: viewZ = Projection._m32 / (ndcDepth - Projection._m22)
float GetLinearDepth(float2 uv)
{
    float d = GDepth.Sample(g_SamplerState, uv).r;
    float denom = d - Projection._m22;
    // |denom| が極端に小さい場合 (d=0.0 付近の遠平面) は near 距離を返す
    if (abs(denom) < 1e-5f) return -Projection._m32 / Projection._m22;
    return Projection._m32 / denom;
}
