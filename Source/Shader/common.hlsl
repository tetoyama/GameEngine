// ==========================================
// COMMON_HLSL / CPP
// C++ と HLSL の両方から共有する定義
// ==========================================
#ifndef COMMON_HLSL
#define COMMON_HLSL

#ifndef COMMON_DEFINE_H
#include "commonDefine.h" 
#endif

#ifdef __cplusplus
#pragma once

// --------------------
// C++ 側
// --------------------
#include <DirectXMath.h>
#include <cstdint>

using float2 = DirectX::XMFLOAT2;
using float3 = DirectX::XMFLOAT3;
using float4 = DirectX::XMFLOAT4;
using float4x4 = DirectX::XMFLOAT4X4;
using matrix = DirectX::XMFLOAT4X4;

using uint = uint32_t;
using int2 = DirectX::XMINT2;
using int3 = DirectX::XMINT3;
using uint3 = DirectX::XMUINT3;
using uint4 = DirectX::XMUINT4;

// C++ 側では定数バッファ宣言を通常構造体へ置き換える
#define CBUFFER(name, slot) struct alignas(16) name
#define REGISTER_B(id)
#define REGISTER_T(id)
#define REGISTER_S(id)
#define REGISTER_U(id)

struct VERTEX_3D {
    DirectX::XMFLOAT3 Position; // 座標 (x, y, z) 
    DirectX::XMFLOAT3 Normal; // 法線 (x, y, z) 
    DirectX::XMFLOAT3 Tangent; // 接線 (x, y, z) 
    DirectX::XMFLOAT4 Diffuse; // カラー (r, g, b, a) 
    DirectX::XMFLOAT2 TexCoord; // UV座標 (u, v) 
};

#else
// --------------------
// HLSL 側
// --------------------
#define CBUFFER(name, slot) cbuffer name : register(b##slot)
#define REGISTER_B(id) register(b##id)
#define REGISTER_T(id) register(t##id)
#define REGISTER_S(id) register(s##id)
#define REGISTER_U(id) register(u##id)

// ------------------------------------------
// ユーティリティ関数
// ------------------------------------------
float2 TransformUV(float2 In, float2 start, float2 end)
{
    return start + In * (end - start);
}

#endif // __cplusplus

// ==========================================
// 共通構造体
// ==========================================

// マテリアル構造体
struct MATERIAL
{
    float4 BaseColor; // Linear RGB, A = Opacity
    float Metallic;
    float Roughness;
    float AO;
    float Pad0;

    float3 EmissiveColor;
    float EmissiveIntensity;

    uint MaterialFlags;
    uint3 Pad1;
};

// ライト構造体
// ShadowMapPass が GPU 向けに整形したライト配列をそのまま参照する
struct LIGHT
{
    int Enable;
    int LightType;
    int CastShadow;
    // CSM カスケードマーカー:
    //   0 = 通常ライト (Point / Spot / Directional 非 CSM)
    //   1 = CSM 最精細カスケード (cascade index 0)
    //   2..N = CSM 2 番目以降のカスケード (cascade index Dummy-1)
    // ※ GPU ライトバッファ上に LIGHT_TYPE_DIRECTIONAL_CSM は存在しない。
    //   ShadowMapPass が N 個の LIGHT_TYPE_DIRECTIONAL エントリに展開する。
    int Dummy;

    // Position.xyz : ライト位置 (Point/Spot) / CSM カスケードのライトカメラ eye 座標
    // Position.w   : Dummy==1 のカスケード先頭エントリのみ使用 → カスケード総数 (float)
    float4 Position;
    // Direction.xyz : ライト方向 (Directional/Spot)
    // Direction.w   : 未使用 (常に 0)
    float4 Direction;

    float4 Diffuse;
    float4 Ambient;

    float4x4 LightView;
    float4x4 LightProjection;

    // x: 範囲(Point/Spot), y: 内側コーン角度(Spot), z: 外側コーン角度(Spot), w: シャドウバイアス
    float4 Param;
};

// ==========================================
// 定数バッファ (更新頻度順)
// ==========================================

// b0: フレームごとに更新 (ライト情報)
CBUFFER(CbPerFrame, 0)
{
    int ActiveLightCount;
    int ShadowAtlasCount;
    int2 _LightPad;
    LIGHT Lights[LIGHT_MAX_COUNT];
};

// b1: カメラ/パスごとに更新 (ビュー・プロジェクション・カメラ位置)
CBUFFER(CbPerCamera, 1)
{
    float4x4 View;
    float4x4 Projection;
    float4 CameraPosition;
};

// b2: オブジェクトごとに更新 (ワールド・マテリアル・UV・パラメータ・オブジェクト情報)
CBUFFER(CbPerObject, 2)
{
    float4x4 World;
    MATERIAL Material;
    float2 UVStart;
    float2 UVEnd;
    float4 Parameter;
    uint SceneID;
    uint ObjectID;
    uint ShaderID;
    uint _ObjPad;
};

#ifdef __cplusplus
// UV スライス情報 (UVStart/UVEnd の 2フィールドをまとめる)
struct UVMatrixBuffer { float2 UVStart; float2 UVEnd; };
// オブジェクト識別情報 (SceneID / ObjectID / ShaderID をまとめる)
struct ObjectInfo     { uint SceneID; uint ObjectID; uint ShaderID; uint _pad; };
// LightBuffer は CbPerFrame と同一 (親しみのある型名を維持)
using LightBuffer = CbPerFrame;
#endif
#ifndef __cplusplus

// ------------------------------------------
// シェーダ入力構造体
// ------------------------------------------
struct VS_IN
{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct PS_IN
{
    float4 Position : SV_POSITION;

    float4 Normal : NORMAL0;
    float4 Tangent : TANGENT0;
    float4 Bitangent : BINORMAL0;

    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float4 WorldPosition : TEXCOORD1;
};

struct PS_OUT
{
    float4 Color : SV_Target0;
    float4 Emissive : SV_Target1;
    float4 Normal : SV_Target2;
};

#endif // __cplusplus

#endif // COMMON_HLSL
