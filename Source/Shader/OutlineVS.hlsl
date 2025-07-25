#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    float OutlineThickness = 0.1f;

    // 頂点をワールド空間へ変換
    float4 worldPos = mul(In.Position, World);
    float4 normal = float4(In.Normal.xyz, 0.0f);
    float4 worldNormal = normalize(mul(normal, World));

    // ワールド空間 → ビュー空間
    float4 viewPos = mul(worldPos, View);
    float3 viewNormal = normalize(mul((float3x3) View, worldNormal.xyz));

    // ビュー空間で押し出し
    viewPos.xyz += viewNormal * OutlineThickness;

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
}
