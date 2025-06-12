
#include "common.hlsl"


void main(in VS_IN In, out PS_IN Out)
{

    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);

    float2 animatedUV = TransformUV(In.TexCoord, UVStart,UVEnd);
    
    Out.Normal = In.Normal;
    Out.WorldPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    Out.Position = mul(In.Position, wvp);
    Out.TexCoord = animatedUV;
    Out.Diffuse = In.Diffuse;
}

