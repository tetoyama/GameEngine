#pragma once
#include "../common.hlsl"
#include "../commonDefine.h"

//======================================================
// G-Buffer 定義
//======================================================
// GBuffer テクスチャ - ポストエフェクト用固定スロット (see PostEffectGBufferSlot_* in commonDefine.h)
Texture2D GAlbedo   : register(t8);   // PostEffectGBufferSlot_Albedo
Texture2D GNormal   : register(t9);   // PostEffectGBufferSlot_Normal
Texture2D GPosition : register(t10);  // PostEffectGBufferSlot_Position
Texture2D GMaterial : register(t11);  // PostEffectGBufferSlot_Material
Texture2D GEmissive : register(t12);  // PostEffectGBufferSlot_Emissive
Texture2D<uint4> GParam : register(t13); // PostEffectGBufferSlot_Param

SamplerState LinearSampler : register(s0);

//======================================================
// 汎用出力構造体（MRT対応）
//======================================================
struct POST_PS_OUT
{
    float4 Color : SV_Target0; // 最終画面表示
    float4 Normal : SV_Target1; // 中間バッファ出力用（必要な場合）
    float4 Emissive : SV_Target2; // 中間バッファ出力用（必要な場合）
};

//======================================================
// G-Buffer から情報を取得する関数群
//======================================================

// 法線復元 [-1,1] に変換
float3 GetWorldNormal(float2 TexCoord)
{
    float4 normal = GNormal.Sample(LinearSampler, TexCoord);
    return normal.xyz * 2.0 - 1.0;
}

// ワールド座標取得
float3 GetWorldPosition(float2 TexCoord)
{
    float4 pos = GPosition.Sample(LinearSampler, TexCoord);
    return pos.xyz;
}

// アルベド取得
float4 GetAlbedo(float2 TexCoord)
{
    return GAlbedo.Sample(LinearSampler, TexCoord);
}

// マテリアルパラメータ取得
// x: Roughness, y: Metallic, z: AO, w: Flags
float4 GetMaterial(float2 TexCoord)
{
    return GMaterial.Sample(LinearSampler, TexCoord);
}

// エミッシブ取得
float4 GetEmissive(float2 TexCoord)
{
    return GEmissive.Sample(LinearSampler, TexCoord);
}

// 整数パラメータ取得
// x: ObjectID, y: SceneID, z: ShaderID, w: Flags
uint4 GetParam(int2 pixelCoord)
{
    return GParam.Load(int3(pixelCoord, 0));
}

// スクリーン座標 TexCoord → 整数ピクセル座標
int2 TexCoordToPixel(float2 TexCoord, float2 GBufferSize)
{
    return int2(TexCoord * GBufferSize);
}
