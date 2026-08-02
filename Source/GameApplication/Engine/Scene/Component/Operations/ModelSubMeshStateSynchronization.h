#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "Resources/Data/modelMaterialTypes.h"

namespace ModelSubMeshStateSynchronization {

inline bool IsDefaultState(const ModelSubMeshRenderState& state) noexcept {
	return state.visible &&
		state.castShadow &&
		state.material.source == SubMeshMaterialSource::ModelDefault &&
		state.material.customMaterialID == InvalidCustomMaterialID;
}

inline void SanitizeMaterialAssignment(
	SubMeshMaterialAssignment& assignment
) noexcept {
	if(assignment.source == SubMeshMaterialSource::CustomMaterial &&
		assignment.customMaterialID != InvalidCustomMaterialID){
		return;
	}
	assignment.source = SubMeshMaterialSource::ModelDefault;
	assignment.customMaterialID = InvalidCustomMaterialID;
}

inline void Synchronize(
	std::vector<ModelSubMeshRenderState>& states,
	std::span<const ModelSubMeshDefinition> definitions
){
	std::unordered_map<ModelSubMeshID, ModelSubMeshRenderState> existing;
	existing.reserve(states.size());
	for(ModelSubMeshRenderState state : states){
		if(state.subMeshID == InvalidModelSubMeshID){
			continue;
		}
		SanitizeMaterialAssignment(state.material);
		existing.try_emplace(state.subMeshID, std::move(state));
	}

	std::vector<ModelSubMeshRenderState> synchronized;
	synchronized.reserve(definitions.size());
	for(const ModelSubMeshDefinition& definition : definitions){
		if(definition.id == InvalidModelSubMeshID){
			continue;
		}

		ModelSubMeshRenderState state;
		state.subMeshID = definition.id;
		if(const auto found = existing.find(definition.id);
			found != existing.end()){
			state = found->second;
			state.subMeshID = definition.id;
		}
		synchronized.push_back(std::move(state));
	}
	states = std::move(synchronized);
}

} // namespace ModelSubMeshStateSynchronization
