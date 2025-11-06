
#include "common.hlsl"

Texture2D g_Texture : register(t0); //テクスチャ０番
SamplerState g_SamplerState : register(s0); //サンプラー０番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //光源からピクセルへのベクトル
    float3 LightVec = normalize(In.WorldPosition.xyz - Lights[0].Position.xyz);
    
    //光源計算
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, LightVec);
    
    //距離による減衰
    float LightDistance = length(In.WorldPosition.xyz - Lights[0].Position.xyz);
    float ofs = 1.0f - (1.0f / Lights[0].PointLightParam.x) * LightDistance;
    light *= ofs;
    
    //視線ベクトル
    float3 EyeVec = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    
    //反射ベクトル
    float3 RefVec = normalize(reflect(normalize(Lights[0].Direction.xyz), normal.xyz));
    
    //スペキュラの計算
    float Specular = -dot(EyeVec, RefVec);
    Specular = pow(saturate(Specular), 30.0f);
    
    //コーンの向きとLightVecの角度
    float Angle = acos(dot(LightVec.xyz, normalize(Lights[0].Direction.xyz)));
    
    //angleがコーンの中にどのくらい入っているか？
    float Spot = saturate(1.0f - 1.0f / Lights[0].Angle.x * abs(Angle));
    Spot += Lights[0].PointLightParam.y;
    
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * Material.Diffuse;
    outDiffuse.rgb *= Lights[0].Diffuse.rgb * In.Diffuse.rgb * light * Spot + Lights[0].Ambient.rgb;
    outDiffuse.rgb += (Specular * Spot);
    
    outDiffuse.rgb = outDiffuse.rgb * Material.TextureEnable + (!Material.TextureEnable * Material.Diffuse.rgb);

}
