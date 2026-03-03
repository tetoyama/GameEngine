#include "../common.hlsl"
#include "PostEffectFunc.hlsli"

// ライティング済みシーンテクスチャ (t0)
Texture2D g_Texture : register(t0);

static const float SSR_SCHLICK_EXPONENT = 5.0f;
static const float SSR_DIELECTRIC_F0 = 0.04f;

// Parameter.x : 反射強度 (推奨 1.0)
// Parameter.y : 最大レイ長 (view-space, 推奨 40.0)
// Parameter.z : レイマーチステップ数 (推奨 32)
// Parameter.w : ヒット厚み (view-space, 推奨 0.2)
void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float2 uv = In.TexCoord;
    float4 sceneColor = g_Texture.Sample(g_SamplerState, uv);

    if (GetDepth(uv) >= 1.0f)
    {
        outDiffuse = sceneColor;
        return;
    }

    float4 material = GetMaterial(uv);
    float metallic = saturate(material.x);
    if (metallic <= 0.001f)
    {
        outDiffuse = sceneColor;
        return;
    }

    float3 worldPos = GetWorldPosition(uv);
    float3 worldNormal = normalize(GetWorldNormal(uv));
    float3 viewDirWS = normalize(CameraPosition.xyz - worldPos);
    float3 reflDirWS = normalize(reflect(-viewDirWS, worldNormal));

    float3 viewPos = mul(float4(worldPos, 1.0f), View).xyz;
    float3 reflDirVS = normalize(mul(float4(reflDirWS, 0.0f), View).xyz);

    int stepCount = max((int)Parameter.z, 1);
    float maxDistance = max(Parameter.y, 0.001f);
    float stepLength = maxDistance / stepCount;
    float thickness = max(Parameter.w, 0.001f);

    float3 reflectedColor = sceneColor.rgb;
    bool hit = false;

    [loop]
    for (int i = 1; i <= stepCount; ++i)
    {
        float3 samplePosVS = viewPos + reflDirVS * (stepLength * i);
        float4 sampleClip = mul(float4(samplePosVS, 1.0f), Projection);
        if (sampleClip.w <= 0.0f) continue;

        float2 sampleUV = float2(
            sampleClip.x / sampleClip.w * 0.5f + 0.5f,
            -sampleClip.y / sampleClip.w * 0.5f + 0.5f
        );

        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        float3 sceneWorldPos = GetWorldPosition(sampleUV);
        float sceneDepthVS = mul(float4(sceneWorldPos, 1.0f), View).z;
        float depthDiff = sceneDepthVS - samplePosVS.z;

        if (depthDiff <= 0.0f && abs(depthDiff) <= thickness)
        {
            reflectedColor = g_Texture.Sample(g_SamplerState, sampleUV).rgb;
            hit = true;
            break;
        }
    }

    float fresnel = pow(1.0f - saturate(dot(worldNormal, viewDirWS)), SSR_SCHLICK_EXPONENT);
    float reflection = saturate(Parameter.x) * metallic * (SSR_DIELECTRIC_F0 + (1.0f - SSR_DIELECTRIC_F0) * fresnel);
    outDiffuse = float4(lerp(sceneColor.rgb, reflectedColor, hit ? reflection : 0.0f), sceneColor.a);
}
