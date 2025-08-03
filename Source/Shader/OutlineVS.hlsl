#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    float OutlineThickness = 0.05f;

    // 頂点をワールド空間へ変換
    float4 worldPos = mul(In.Position, World);
    float4 normal = float4(In.Normal.xyz, 0.0f);
    float4 worldNormal = normalize(mul(normal, World));

    float distance = length(CameraPosition.xyz - In.Position.xyz);
    float thicknessScale = clamp(1.0f / distance, 0.01f, 1.0f); // 距離によって縮小
    
    worldPos.xyz += worldNormal * OutlineThickness * thicknessScale;

    // ワールド空間 → ビュー空間
    float4 viewPos = mul(worldPos, View);
    float3 viewNormal = normalize(mul((float3x3) View, worldNormal.xyz));
    
    // ビュー→プロジェクションへ
    Out.Position = mul(viewPos, Projection);

    // ワールド空間での法線・タンジェント等をそのまま使用
    Out.Normal = worldNormal;

    float4 tangent = float4(In.Tangent.xyz, 0.0f);
    float4 worldTangent = normalize(mul(tangent, World));
    Out.Tangent = worldTangent;

    float3 worldNormal3 = normalize(worldNormal.xyz);
    float3 worldTangent3 = normalize(worldTangent.xyz);
    float3 worldBitangent3 = cross(worldNormal3, worldTangent3) * In.Tangent.w;
    Out.Bitangent = float4(worldBitangent3, 0.0f);

    Out.Diffuse = In.Diffuse;
    Out.TexCoord = TransformUV(In.TexCoord, UVStart, UVEnd);
    Out.WorldPosition = worldPos; // 元の位置
    
    // ワールド空間での法線・タンジェント等をそのまま使用
    Out.Normal = worldNormal;
    Out.Tangent = worldTangent;
    Out.Bitangent = float4(worldBitangent3, 0.0f);

    Out.Diffuse = In.Diffuse;
    Out.TexCoord = TransformUV(In.TexCoord, UVStart, UVEnd);
    Out.WorldPosition = worldPos; // 元の位置

}
