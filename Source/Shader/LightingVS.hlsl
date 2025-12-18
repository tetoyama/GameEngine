#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    Out.Position = float4(In.Position.xyz, 1.0f);
    //Out.TexCoord = In.TexCoord;
    Out.TexCoord = float2(
        (In.Position.x * 0.5f) + 0.5f,
        (-In.Position.y * 0.5f) + 0.5f
    );
    
    Out.Normal = float4(0, 0, 0, 0);
    Out.Tangent = float4(0, 0, 0, 0);
    Out.Bitangent = float4(0, 0, 0, 0);
    Out.Diffuse = float4(1, 1, 1, 1);
    Out.WorldPosition = float4(0, 0, 0, 0);
}
