struct VS_IN
{
    float3 Position : POSITION0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUT
{
    float3 LocalPos : TEXCOORD0;
};

VS_OUT main(VS_IN In)
{
    VS_OUT Out;
    Out.LocalPos = In.Position.xyz;
    return Out;
}
