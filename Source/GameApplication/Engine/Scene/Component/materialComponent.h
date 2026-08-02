// =======================================================================
//
// materialComponent.h
//
// =======================================================================
#pragma once

#include <vector>

#include "Interface/IComponent.h"
#include "Resources/Data/modelMaterialTypes.h"
#include "Shader/Common.hlsl"
#include "Shader/CommonDefine.h"

// ユーザー定義MaterialをEntity単位で保持するComponent。
// 旧ShaderID / MATERIALは移行期間中の単一Material互換経路として維持する。
class MaterialComponent {
public:
	int ShaderID = 0;
	MATERIAL Material{};

	// Custom Materialの定義だけを所有する。
	// SubMeshへの適用範囲はModelRendererComponentが所有する。
	std::vector<CustomMaterialEntry> materials;

	MaterialComponent(){
		Material.BaseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	const CustomMaterialEntry* FindMaterial(
		CustomMaterialID id
	) const noexcept {
		if(id == InvalidCustomMaterialID){
			return nullptr;
		}
		for(const CustomMaterialEntry& material : materials){
			if(material.id == id){
				return &material;
			}
		}
		return nullptr;
	}

	CustomMaterialEntry* FindMaterial(
		CustomMaterialID id
	) noexcept {
		return const_cast<CustomMaterialEntry*>(
			static_cast<const MaterialComponent*>(this)->FindMaterial(id)
		);
	}

	YAML::Node encode();
	bool decode(SceneContext* context, const YAML::Node& node);
	void inspector(SceneContext* context);
};

#include "Operations/MaterialComponentOperations.h"