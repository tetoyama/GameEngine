#ifndef COMMON_HLSL
#define COMMON_HLSL

#ifndef COMMON_DEFINE_H
#include "commonDefine.h"
#endif

cbuffer WorldBuffer : register(b0)
{
    matrix World;
}
cbuffer ViewBuffer : register(b1)
{
    matrix View;
}
cbuffer ProjectionBuffer : register(b2)
{
    matrix Projection;
}

struct MATERIAL
{
    float4 BaseColor; // Linear RGB, A = Opacity

    float Metallic;
    float Roughness;
    float AO;
    float Pad0;

    float3 EmissiveColor;
    float EmissiveIntensity;

    uint MaterialFlags; // UseBaseColorMap, UseNormalMap, etc.
    uint3 Pad1;
};


cbuffer MaterialBuffer : register(b3)
{
    MATERIAL Material;
}

cbuffer UVMatrixBuffer : register(b4)
{
    float2 UVStart;
    float2 UVEnd;
};

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

cbuffer LightBuffer : register(b5)
{
    int ActiveLightCount;
    int ShadowAtlasCount;
    int2 Dummy;
    LIGHT Lights[LIGHT_MAX_COUNT]; // LIGHT_MAX_COUNT
}

// ViewBufferとまとめられる?
cbuffer CameraBuffer : register(b6)
{
    float4 CameraPosition;
}

cbuffer ParameterBuffer : register(b7)
{
    float4 Parameter;
};

cbuffer ObjectInfo : register(b8)
{
    uint SceneID; // drawcallごとに設定
    uint ObjectID; // drawcallごとに設定
    uint ShaderID; // マテリアルごとに設定
    uint _pad;
}

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

float2 TransformUV(float2 In, float2 start, float2 end)
{
    return start + In * (end - start);
}

#endif // COMMON_HLSL
