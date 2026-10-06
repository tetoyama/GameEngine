cbuffer Frame : register(b0, space1)
{
    row_major float4x4 frame_viewProjection : packoffset(c0);
    row_major float4x4 frame_lightViewProjection : packoffset(c4);
    float4 frame_lightDirection : packoffset(c8);
    float4 frame_lightColor : packoffset(c9);
    float4 frame_ambientColor : packoffset(c10);
    float4 frame_outputTransform : packoffset(c11);
};


static float4 gl_Position;
static float4 world0;
static float4 world1;
static float4 world2;
static float4 world3;
static float3 position;
static float3 worldPosition;
static float3 worldNormal;
static float3 normal;
static float4 albedo;
static float4 color;
static float2 uv;
static float2 texcoord;
static float4 uvTransform;
static float unlit;
static float4 shading;

struct SPIRV_Cross_Input
{
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 world0 : TEXCOORD2;
    float4 world1 : TEXCOORD3;
    float4 world2 : TEXCOORD4;
    float4 world3 : TEXCOORD5;
    float4 color : TEXCOORD6;
    float2 texcoord : TEXCOORD7;
    float4 uvTransform : TEXCOORD8;
    float4 shading : TEXCOORD9;
};

struct SPIRV_Cross_Output
{
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 albedo : TEXCOORD2;
    float2 uv : TEXCOORD3;
    float unlit : TEXCOORD4;
    float4 gl_Position : SV_Position;
};

// Returns the determinant of a 2x2 matrix.
float spvDet2x2(float a1, float a2, float b1, float b2)
{
    return a1 * b2 - b1 * a2;
}

// Returns the inverse of a matrix, by using the algorithm of calculating the classical
// adjoint and dividing by the determinant. The contents of the matrix are changed.
float3x3 spvInverse(float3x3 m)
{
    float3x3 adj;	// The adjoint matrix (inverse after dividing by determinant)

    // Create the transpose of the cofactors, as the classical adjoint of the matrix.
    adj[0][0] =  spvDet2x2(m[1][1], m[1][2], m[2][1], m[2][2]);
    adj[0][1] = -spvDet2x2(m[0][1], m[0][2], m[2][1], m[2][2]);
    adj[0][2] =  spvDet2x2(m[0][1], m[0][2], m[1][1], m[1][2]);

    adj[1][0] = -spvDet2x2(m[1][0], m[1][2], m[2][0], m[2][2]);
    adj[1][1] =  spvDet2x2(m[0][0], m[0][2], m[2][0], m[2][2]);
    adj[1][2] = -spvDet2x2(m[0][0], m[0][2], m[1][0], m[1][2]);

    adj[2][0] =  spvDet2x2(m[1][0], m[1][1], m[2][0], m[2][1]);
    adj[2][1] = -spvDet2x2(m[0][0], m[0][1], m[2][0], m[2][1]);
    adj[2][2] =  spvDet2x2(m[0][0], m[0][1], m[1][0], m[1][1]);

    // Calculate the determinant as a combination of the cofactors of the first row.
    float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]) + (adj[0][2] * m[2][0]);

    // Divide the classical adjoint matrix by the determinant.
    // If determinant is zero, matrix is not invertable, so leave it unchanged.
    return (det != 0.0f) ? (adj * (1.0f / det)) : m;
}

void vert_main()
{
    float4x4 world = float4x4(float4(world0), float4(world1), float4(world2), float4(world3));
    float4 p = mul(float4(position, 1.0f), world);
    worldPosition = p.xyz;
    worldNormal = mul(normal, transpose(spvInverse(float3x3(world[0].xyz, world[1].xyz, world[2].xyz))));
    albedo = color;
    uv = (texcoord * uvTransform.xy) + uvTransform.zw;
    unlit = shading.x;
    gl_Position = mul(p, frame_viewProjection);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    world0 = stage_input.world0;
    world1 = stage_input.world1;
    world2 = stage_input.world2;
    world3 = stage_input.world3;
    position = stage_input.position;
    normal = stage_input.normal;
    color = stage_input.color;
    texcoord = stage_input.texcoord;
    uvTransform = stage_input.uvTransform;
    shading = stage_input.shading;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.worldPosition = worldPosition;
    stage_output.worldNormal = worldNormal;
    stage_output.albedo = albedo;
    stage_output.uv = uv;
    stage_output.unlit = unlit;
    return stage_output;
}
