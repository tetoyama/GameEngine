Texture2D<float4> baseColorTexture : register(t0, space2);
SamplerState _baseColorTexture_sampler : register(s0, space2);

static float4 outAlbedo;
static float4 albedo;
static float2 uv;
static float4 outNormal;
static float3 worldNormal;
static float unlit;
static float4 outPosition;
static float3 worldPosition;

struct SPIRV_Cross_Input
{
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 albedo : TEXCOORD2;
    float2 uv : TEXCOORD3;
    float unlit : TEXCOORD4;
};

struct SPIRV_Cross_Output
{
    float4 outAlbedo : SV_Target0;
    float4 outNormal : SV_Target1;
    float4 outPosition : SV_Target2;
};

void frag_main()
{
    outAlbedo = albedo * baseColorTexture.Sample(_baseColorTexture_sampler, uv);
    outNormal = float4(normalize(worldNormal), unlit);
    outPosition = float4(worldPosition, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    albedo = stage_input.albedo;
    uv = stage_input.uv;
    worldNormal = stage_input.worldNormal;
    unlit = stage_input.unlit;
    worldPosition = stage_input.worldPosition;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outAlbedo = outAlbedo;
    stage_output.outNormal = outNormal;
    stage_output.outPosition = outPosition;
    return stage_output;
}
