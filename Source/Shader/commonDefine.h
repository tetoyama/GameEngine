// =======================================================================
// 
// commonDefine.h
// 
// =======================================================================
#ifdef __cplusplus
// C++ Build

#else
// HLSL Build

#endif

#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

#define ALPHA_CLIP_THRESHOLD        (0.01f)
#define PI                          (3.14159265359f)
#define DEPTH_BIAS_CONSTANT         (0.005f)
#define DEPTH_SLOPE_BIAS            (0.003f)

// Point / Spot ShadowのPerspective Clip契約。
// C++生成側とHLSL Sampling側で同じ値を使用する。
#define LOCAL_LIGHT_SHADOW_NEAR_PLANE            (0.1f)
#define LOCAL_LIGHT_SHADOW_MIN_DEPTH_SPAN        (0.1f)
#define LOCAL_LIGHT_SHADOW_MAX_NDC_BIAS          (0.01f)
// 既存Param.wのNDC Biasを、距離に依存しないWorld Biasへ変換する基準深度。
// 深度1.0までは旧挙動を上限として維持し、それ以遠は1/depth^2で弱める。
#define LOCAL_LIGHT_SHADOW_BIAS_REFERENCE_DEPTH  (1.0f)
#define LOCAL_LIGHT_SHADOW_MAX_SLOPE_SCALE       (9.0f)

#define BONE_MAX_COUNT          (256)

#define LOW_RESOLUTION

#define TextureSlot_Albedo      (0)
#define TextureSlot_Normal      (1)
#define TextureSlot_Roughness   (2)
#define TextureSlot_Metallic    (3)
#define TextureSlot_AO          (4)
#define TextureSlot_HeightMap   (5)
#define TextureSlot_EmissiveMap (6)
#define TextureSlot_ShadowMap   (7)
#define TextureSlot_EnvironmentMap (8)

#define TextureSlot_Max         (9)

#define GBufferSlot_Albedo      (0)
#define GBufferSlot_Normal      (1)
#define GBufferSlot_Position    (2)
#define GBufferSlot_Material    (3)
#define GBufferSlot_Emissive    (4)
#define GBufferSlot_Param       (5)

#define GBufferSlot_Max         (6)

// GBuffer Param uint4 channel contract shared by ordinary and static paths.
#define GBufferParamChannel_SceneID       (0)
#define GBufferParamChannel_ObjectID      (1)
#define GBufferParamChannel_ShaderID      (2)
#define GBufferParamChannel_MaterialFlags (3)

// Static instance uint4 input contract.
#define StaticInstanceObjectChannel_EntityIndex      (0)
#define StaticInstanceObjectChannel_EntityGeneration (1)
#define StaticInstanceObjectChannel_SceneID          (2)
#define StaticInstanceObjectChannel_Reserved         (3)

#define LightingSlot_GAlbedo    (0)
#define LightingSlot_GNormal    (1)
#define LightingSlot_GPosition  (2)
#define LightingSlot_GMaterial  (3)
#define LightingSlot_GEmissive  (4)
#define LightingSlot_GParam     (5)
#define LightingSlot_ShadowMap  (6)
#define LightingSlot_EnvironmentMap (7)

#define LightingSlot_Max        (8)

// Lighting diagnostic flags. These are uniform per draw and must not be
// used to dispatch material implementations.
#define LIGHTING_DEBUG_FLAG_DISABLE_SHADOWS          (1u << 0)
#define LIGHTING_DEBUG_FLAG_DISABLE_ENVIRONMENT      (1u << 1)
#define LIGHTING_DEBUG_FLAG_DISABLE_CSM_SHADOWS      (1u << 2)
#define LIGHTING_DEBUG_FLAG_DISABLE_POINT_SHADOWS    (1u << 3)

// PCF override modes. 0 preserves each material's default kernel.
#define LIGHTING_DEBUG_PCF_DEFAULT (0)
#define LIGHTING_DEBUG_PCF_1X1     (1)
#define LIGHTING_DEBUG_PCF_3X3     (2)
#define LIGHTING_DEBUG_PCF_5X5     (3)

// ポストエフェクト用 GBuffer スロット (フォワードパスのテクスチャスロット後に続く固定スロット)
#define PostEffectGBufferSlot_Start     (8)
#define PostEffectGBufferSlot_Albedo    (8)
#define PostEffectGBufferSlot_Normal    (9)
#define PostEffectGBufferSlot_Position  (10)
#define PostEffectGBufferSlot_Material  (11)
#define PostEffectGBufferSlot_Emissive  (12)
#define PostEffectGBufferSlot_Param     (13)
#define PostEffectGBufferSlot_Depth     (14)
#define PostEffectGBufferSlot_Count     (7)
#define PostEffectTextureSlot_Max       (PostEffectGBufferSlot_Start + PostEffectGBufferSlot_Count)

#define LIGHT_TYPE_NONE             (0)
#define LIGHT_TYPE_DIRECTIONAL      (1)
#define LIGHT_TYPE_POINT            (2)
#define LIGHT_TYPE_SPOT             (3)
// LightComponent のユーザー向けライトタイプ識別子。
// ShadowMapPassはCSMを複数GPU Entryへ展開する。
#define LIGHT_TYPE_DIRECTIONAL_CSM  (4)

#define DIRECTIONAL_CSM_CASCADE_COUNT   (4)

// MaterialFlags 用のビットマスク
#define MATERIAL_FLAG_USE_DIFFUSE_TEXTURE       (1 << 0)
#define MATERIAL_FLAG_USE_NORMAL_TEXTURE        (1 << 1)
#define MATERIAL_FLAG_USE_ROUGHNESS_TEXTURE     (1 << 2)
#define MATERIAL_FLAG_USE_METALLIC_TEXTURE      (1 << 3)
#define MATERIAL_FLAG_USE_ENVIRONMENT_MAP       (1 << 4)

#define LIGHT_MAX_COUNT         (64)

#ifdef LOW_RESOLUTION
#define SHADOWMAP_SIZE          (2048)
#else
#define SHADOWMAP_SIZE          (16384)
#endif // LOW_RESOLUTION

#ifndef __cplusplus
uint4 MakeGBufferParameter(
    uint sceneID,
    uint objectID,
    uint shaderID,
    uint materialFlags)
{
    uint4 parameter = uint4(0, 0, 0, 0);
    parameter[GBufferParamChannel_SceneID] = sceneID;
    parameter[GBufferParamChannel_ObjectID] = objectID;
    parameter[GBufferParamChannel_ShaderID] = shaderID;
    parameter[GBufferParamChannel_MaterialFlags] = materialFlags;
    return parameter;
}
#endif

#endif // !COMMON_DEFINE_H