cbuffer Frame : register(b0, space1)
{
    row_major float4x4 frame_viewProjection : packoffset(c0);
    row_major float4x4 frame_lightViewProjection : packoffset(c4);
    float4 frame_lightDirection : packoffset(c8);
    float4 frame_lightColor : packoffset(c9);
    float4 frame_ambientColor : packoffset(c10);
    float4 frame_outputTransform : packoffset(c11);
    float4 frame_cameraPosition : packoffset(c12);
};


static float4 gl_Position;
static float4 world0;
static float4 world1;
static float4 world2;
static float4 world3;
static float3 position;

struct SPIRV_Cross_Input
{
    float3 position : TEXCOORD0;
    float4 world0 : TEXCOORD2;
    float4 world1 : TEXCOORD3;
    float4 world2 : TEXCOORD4;
    float4 world3 : TEXCOORD5;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position = mul(float4(position, 1.0f), mul(float4x4(float4(world0), float4(world1), float4(world2), float4(world3)), frame_lightViewProjection));
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    world0 = stage_input.world0;
    world1 = stage_input.world1;
    world2 = stage_input.world2;
    world3 = stage_input.world3;
    position = stage_input.position;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
