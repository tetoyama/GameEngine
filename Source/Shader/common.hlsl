// ==========================================
// COMMON_HLSL / CPP
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

// マクロ置換
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
// 共有構造体
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
struct LIGHT
{
    int Enable;
    int LightType;
    int CastShadow;
    int Dummy;

    float4 Position;
    float4 Direction;

    float4 Diffuse;
    float4 Ambient;

    float4x4 LightView;
    float4x4 LightProjection;

    float4 Param;
};

// ==========================================
// 定数バッファ (更新タイミング別)
// ==========================================

// フレームごとに更新 (ライト情報)
CBUFFER(CbPerFrame, 0)
{
    int ActiveLightCount;
    int ShadowAtlasCount;
    int2 Dummy;
    LIGHT Lights[LIGHT_MAX_COUNT];
};

// カメラ/パスごとに更新 (ビュー・プロジェクション・カメラ位置)
CBUFFER(CbPerCamera, 1)
{
    float4x4 View;
    float4x4 Projection;
    float4 CameraPosition;
};

// オブジェクトごとに更新 (ワールド・マテリアル・UV・オブジェクト情報・パラメータ)
CBUFFER(CbPerObject, 2)
{
    float4x4 World;
    MATERIAL Material;
    float2 UVStart;
    float2 UVEnd;
    uint SceneID;
    uint ObjectID;
    uint ShaderID;
    uint _pad;
    float4 Parameter;
};

#ifdef __cplusplus
// ==========================================
// C++ ヘルパー構造体 (後方互換性のため維持)
// ==========================================

// LightBuffer は CbPerFrame の別名
using LightBuffer = CbPerFrame;

struct CameraBuffer
{
    float4 CameraPosition{ 0.0f, 0.0f, 0.0f, 0.0f };
};

struct UVMatrixBuffer
{
    float2 UVStart{ 0.0f, 0.0f };
    float2 UVEnd{ 1.0f, 1.0f };
};

struct ObjectInfo
{
    uint SceneID = 0;
    uint ObjectID = 0;
    uint ShaderID = 0;
    uint _pad = 0;
};

struct ParameterBuffer
{
    float4 Parameter{ 0.0f, 0.0f, 0.0f, 0.0f };
};
#endif // __cplusplus
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
    float4 Emmisive : SV_Target1;
    float4 Normal : SV_Target2;
};

#endif // __cplusplus

#endif // COMMON_HLSL