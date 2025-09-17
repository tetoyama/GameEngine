#include "../common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{

    Out.Position = In.Position;
    Out.TexCoord = In.TexCoord;

    Out.Normal = float4(0, 0, 0, 0);
    Out.Tangent = float4(0, 0, 0, 0);
    Out.Bitangent = float4(0, 0, 0, 0);
    Out.Diffuse = float4(1, 1, 1, 1);
    Out.WorldPosition = float4(0, 0, 0, 0);
}

