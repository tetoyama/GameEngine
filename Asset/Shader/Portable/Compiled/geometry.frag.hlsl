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
static float4 outMaterial;
static float4 material;
static float4 outEmissive;
static float4 emissive;

struct SPIRV_Cross_Input
{
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 albedo : TEXCOORD2;
    float2 uv : TEXCOORD3;
    float unlit : TEXCOORD4;
    float4 material : TEXCOORD5;
    float4 emissive : TEXCOORD6;
};

struct SPIRV_Cross_Output
{
    float4 outAlbedo : SV_Target0;
    float4 outNormal : SV_Target1;
    float4 outPosition : SV_Target2;
    float4 outMaterial : SV_Target3;
    float4 outEmissive : SV_Target4;
};

void frag_main()
{
    outAlbedo = albedo * baseColorTexture.Sample(_baseColorTexture_sampler, uv);
    if (outAlbedo.w < 0.00999999977648258209228515625f)
    {
        discard;
    }
    outNormal = float4(normalize(worldNormal), unlit);
    outPosition = float4(worldPosition, 1.0f);
    outMaterial = material;
    outEmissive = emissive;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    albedo = stage_input.albedo;
    uv = stage_input.uv;
    worldNormal = stage_input.worldNormal;
    unlit = stage_input.unlit;
    worldPosition = stage_input.worldPosition;
    material = stage_input.material;
    emissive = stage_input.emissive;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outAlbedo = outAlbedo;
    stage_output.outNormal = outNormal;
    stage_output.outPosition = outPosition;
    stage_output.outMaterial = outMaterial;
    stage_output.outEmissive = outEmissive;
    return stage_output;
}
