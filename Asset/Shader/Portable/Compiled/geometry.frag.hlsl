static float4 outAlbedo;
static float4 albedo;
static float4 outNormal;
static float3 worldNormal;
static float4 outPosition;
static float3 worldPosition;

struct SPIRV_Cross_Input
{
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 albedo : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 outAlbedo : SV_Target0;
    float4 outNormal : SV_Target1;
    float4 outPosition : SV_Target2;
};

void frag_main()
{
    outAlbedo = albedo;
    outNormal = float4(normalize(worldNormal), 1.0f);
    outPosition = float4(worldPosition, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    albedo = stage_input.albedo;
    worldNormal = stage_input.worldNormal;
    worldPosition = stage_input.worldPosition;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.outAlbedo = outAlbedo;
    stage_output.outNormal = outNormal;
    stage_output.outPosition = outPosition;
    return stage_output;
}
