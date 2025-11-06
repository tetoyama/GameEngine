#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

float CalculateGeometricDamping(float nh, float nv, float nl, float vh);
float CalculateDiffuseFromFresnel(float3 N, float3 L, float3 V);
float3 CalculateCookTorranceSpecular(float3 N, float3 L, float3 V, float smooth, float metallic);
float CalculateBeckmann(float m, float t);
float CalculateFresnel(float F0, float u);

void main(in PS_IN In, out float4 outColor : SV_Target)
{
    float3 normal = normalize(In.Normal.xyz);
    
    float4 albedoColor = g_Texture.Sample(g_Sampler, In.TexCoord);

    float4 SpecularColor = albedoColor;
    
    float metallic = 0.5f;
    
    float smoothness = 0.8f;
    
    float3 eyeV = normalize(CameraPosition.xyz - In.WorldPosition.xyz);

    float3 _light = 0;
    
    for(int i = 0; i < 1; i++)
    {
        if (Lights[0].Enable)
        {
            float3 lightV = normalize(Lights[0].Position.xyz - In.WorldPosition.xyz);

            float diffuseFromFresnel = CalculateDiffuseFromFresnel(normal.xyz, lightV, eyeV);
            
            float nl = saturate(dot(normal.xyz, lightV.xyz));
            
            float3 light = nl * Lights[0].Diffuse.rgb / PI;
            
            float3 diffuse = albedoColor.xyz * diffuseFromFresnel * light;
            
            float3 specular = CalculateCookTorranceSpecular(normal.xyz, lightV, eyeV, smoothness, metallic) * Lights[0].Diffuse.rgb;
            
            specular *= lerp(float3(1.0f, 1.0f, 1.0f), SpecularColor.rgb, metallic);
            
            _light += diffuse * (1.0f - smoothness) + specular;
        }
    }
    
    _light += Lights[0].Ambient.rgb * albedoColor.rgb;
    
    outColor = g_Texture.Sample(g_Sampler, In.TexCoord) * Material.Diffuse;
    
    outColor.rgb += _light;
    outColor.a = albedoColor.a * In.Diffuse.a;
}

float CalculateGeometricDamping(float nh, float nv, float nl, float vh)
{
    return min(1.0f, min(2.0f * nh * nv / vh, 2.0f * nh * nl / vh));
}
float CalculateDiffuseFromFresnel(float3 N, float3 L, float3 V)
{
    float nl = saturate(dot(N, L));
    float nv = saturate(dot(N, V));
    
    return (nl * nv);
}
float3 CalculateCookTorranceSpecular(float3 N, float3 L, float3 V, float smooth, float metallic)
{
    float3 H = normalize(L + V);
    
    float nh = saturate(dot(N, H));
    float nl = saturate(dot(N, L));
    float nv = saturate(dot(N, V));
    float vh = saturate(dot(V, H));
    
    float D = CalculateBeckmann(metallic, nh);
    
    float G = CalculateGeometricDamping(nh, nv, nl, vh);
        
    float F = CalculateFresnel(smooth, vh);
    
    float m = PI * nv * nh;
    
    return max(F * D * G / m, 0.0f);
}
float CalculateBeckmann(float smooth, float nh)
{
    float D;
    D = (exp(-(1 - (nh * nh)) / (smooth * smooth * nh * nh)) / (4 * smooth * smooth * nh * nh * nh * nh));
    
    return D;
}
float CalculateFresnel(float F0, float u)
{
    return F0 + (1.0f - F0) * pow(1.0f - u, 5.0f);
}