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

#define ALPHA_CLIP_THRESHOLD		(0.01f)
#define PI							(3.14159265359f)
#define DEPTH_BIAS_CONSTANT			(0.005f)
#define DEPTH_SLOPE_BIAS			(0.003f)

#define BONE_MAX_COUNT			(256)

#define LOW_RESOLUTION

#define TextureSlot_Albedo		(0)
#define TextureSlot_Normal		(1)
#define TextureSlot_Roughness	(2)
#define TextureSlot_Metallic	(3)
#define TextureSlot_AO			(4)
#define TextureSlot_HeightMap	(5)
#define TextureSlot_EmissiveMap	(6)
#define TextureSlot_ShadowMap	(7)
#define TextureSlot_EnvironmentMap (8)

#define TextureSlot_Max			(9)

#define GBufferSlot_Albedo		(0)
#define GBufferSlot_Normal		(1)
#define GBufferSlot_Position	(2)
#define GBufferSlot_Material	(3)
#define GBufferSlot_Emissive	(4)
#define GBufferSlot_Param		(5)

#define GBufferSlot_Max			(6)

#define LightingSlot_GAlbedo    (0)
#define LightingSlot_GNormal    (1)
#define LightingSlot_GPosition  (2)
#define LightingSlot_GMaterial  (3)
#define LightingSlot_GEmissive  (4)
#define LightingSlot_GParam     (5)
#define LightingSlot_ShadowMap  (6)
#define LightingSlot_EnvironmentMap (7)

#define LightingSlot_Max        (8)

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


#define LIGHT_TYPE_NONE				(0)
#define LIGHT_TYPE_DIRECTIONAL		(1)
#define LIGHT_TYPE_POINT			(2)
#define LIGHT_TYPE_SPOT				(3)
// LightComponent のユーザー向けライトタイプ識別子。
// GPU ライトバッファ (CbPerFrame.Lights[]) には LIGHT_TYPE_DIRECTIONAL_CSM エントリは存在しない。
// ShadowMapPass がこのタイプを検出すると DIRECTIONAL_CSM_CASCADE_COUNT 個の
// LIGHT_TYPE_DIRECTIONAL エントリに展開してアトラスに統合する。
#define LIGHT_TYPE_DIRECTIONAL_CSM	(4)

#define DIRECTIONAL_CSM_CASCADE_COUNT	(6)

// MaterialFlags 用のビットマスク
#define MATERIAL_FLAG_USE_DIFFUSE_TEXTURE		(1 << 0)
#define MATERIAL_FLAG_USE_NORMAL_TEXTURE		(1 << 1)
#define MATERIAL_FLAG_USE_ROUGHNESS_TEXTURE		(1 << 2)
#define MATERIAL_FLAG_USE_METALLIC_TEXTURE		(1 << 3)
#define MATERIAL_FLAG_USE_ENVIRONMENT_MAP		(1 << 4)


#ifdef LOW_RESOLUTION
#define SHADOWMAP_SIZE			(2048)
#define LIGHT_MAX_COUNT			(64)
#else
#define SHADOWMAP_SIZE			(16384)
#define LIGHT_MAX_COUNT			(64)
#endif // LOW_RESOLUTION

#endif // !COMMON_DEFINE_H
