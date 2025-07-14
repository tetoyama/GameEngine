
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
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Emission;
    float Shininess;
    bool TextureEnable;
    float2 Dummy;
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
    bool Enable;
    bool3 Dummy;
    
    float4 Direction;
    float4 Diffuse;
    float4 Ambient;
    
    float4 Position;
    float4 PointLightParam;
    float4 Angle;

    float4 SkyColor;
    float4 GroundColor;
    float4 GroundNomal;
};

cbuffer LightBuffer : register(b5)
{
    LIGHT Light;
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

#define MAX_BONES 128

cbuffer AnimationCB : register(b0)
{
    float animationTime;
    int boneCount;
    float2 padding; // 16バイトアライメント調整
    matrix boneMatrices[MAX_BONES];
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

static const float PI = 3.14159265358979323846f;