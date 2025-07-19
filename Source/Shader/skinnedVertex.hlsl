// skinnedVertex.hlsl
struct VertexIn
{
    float3 Position;
    float dummy;
    float3 Normal;
    float dummy2;
    float2 TexCoord;

    uint4 BoneIndices;
    float4 BoneWeights;
};


struct VertexOut
{
    float4 Position;
    float4 Normal;
    float4 Tangent;
    float4 Diffuse;
    float2 TexCoord;
};

StructuredBuffer<VertexIn> g_InputVertices : register(t0);
StructuredBuffer<float4x4> g_BoneMatrices : register(t1); // boneMatrixPalette
RWStructuredBuffer<VertexOut> g_OutputVertices : register(u0);


[numthreads(128, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    VertexIn vin = g_InputVertices[DTid.x];

    float4 skinnedPos = float4(0, 0, 0, 0);
    float4 skinnedNorm = float4(0, 0, 0, 0);

    for (int i = 0; i < 4; ++i)
    {
        uint index = vin.BoneIndices[i];
        float weight = vin.BoneWeights[i];

        float4x4 mat = g_BoneMatrices[index];

        skinnedPos += mul(float4(vin.Position,1.0f), mat) * weight;
        skinnedNorm += mul(float4(vin.Normal, 0.0f), mat) * weight;
    }

    VertexOut vout;
    vout.Position = (skinnedPos.xyz, 0.0f);
    vout.Normal = (normalize(skinnedNorm.xyz), 0.0f);
    vout.Tangent = float4(1.0f, 0.0f, 0.0f,0.0f);
    vout.Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
    vout.TexCoord = vin.TexCoord;

    g_OutputVertices[DTid.x] = vout;
}
