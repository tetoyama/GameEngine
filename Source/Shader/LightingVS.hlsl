#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    Out.Position = float4(In.Position.xyz, 1.0f);
    Out.TexCoord = In.TexCoord;

    Out.Normal = 0;
    Out.Tangent = 0;
    Out.Bitangent = 0;
    Out.Diffuse = 1;
    Out.WorldPosition = 0;
}
