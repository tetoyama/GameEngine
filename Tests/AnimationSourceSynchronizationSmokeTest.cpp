#include <cassert>
#include <string>
#include <unordered_set>

#include "Engine/Scene/System/Render/Animation/AnimationSourceSynchronization.h"

int main(){
	AnimationBindingList::BindingList bindings{
		{"", ""},
		{"MissingAlias", ""},
		{"", "Asset/Animation/Missing.fbx"},
		{"Run", "Asset/Animation/Run.fbx"}
	};

	std::unordered_set<std::string> importedSources;
	int importCalls = 0;
	const bool imported = AnimationSourceSynchronization::Synchronize(
		bindings,
		[&importedSources](const std::string& assetPath){
			return importedSources.contains(assetPath);
		},
		[&](const std::string& alias, const std::string& assetPath){
			assert(alias == "Run");
			assert(assetPath == "Asset/Animation/Run.fbx");
			++importCalls;
			importedSources.insert(assetPath);
		}
	);
	assert(imported);
	assert(importCalls == 1);
	assert(importedSources.contains("Asset/Animation/Run.fbx"));

	// 既にImport済みのSourceは再Importしない。
	const bool importedAgain = AnimationSourceSynchronization::Synchronize(
		bindings,
		[&importedSources](const std::string& assetPath){
			return importedSources.contains(assetPath);
		},
		[&](const std::string&, const std::string&){
			++importCalls;
		}
	);
	assert(!importedAgain);
	assert(importCalls == 1);

	// Import処理後もSourceが利用可能にならない場合は成功扱いしない。
	AnimationBindingList::BindingList failedBinding{
		{"Idle", "Asset/Animation/Idle.fbx"}
	};
	int failedImportCalls = 0;
	const bool failedImportReported = AnimationSourceSynchronization::Synchronize(
		failedBinding,
		[](const std::string&){ return false; },
		[&](const std::string&, const std::string&){ ++failedImportCalls; }
	);
	assert(!failedImportReported);
	assert(failedImportCalls == 1);
	return 0;
}