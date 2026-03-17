struct PS_IN
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
};

cbuffer WaterTessellationCB : register(b3)
{
    float4x4 WT_World;
    float4x4 WT_View;
    float4x4 WT_Projection;
    float3 CameraPos;
    float Time;
    float TessellationMin;
    float TessellationMax;
    float TessellationMinDistance;
    float TessellationMaxDistance;
    float WaveHeight;
    float WaveScale;
    float2 FlowDir1;
    float2 FlowDir2;
    float FlowSpeed1;
    float FlowSpeed2;
    float NormalDelta;
    float FresnelPower;
};

float4 main(PS_IN In) : SV_Target0
{
    float3 N = normalize(In.Normal);
    float3 V = normalize(CameraPos - In.WorldPosition);
    float fresnel = pow(1.0f - saturate(dot(N, V)), FresnelPower);

    float3 waterColor = float3(0.02f, 0.18f, 0.26f);
    float3 reflectionColor = float3(0.35f, 0.54f, 0.74f);
    float3 color = lerp(waterColor, reflectionColor, saturate(0.12f + fresnel));

    return float4(color, 0.82f);
}
