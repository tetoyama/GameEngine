// =======================================================================
//
// SceneStorageYaml.h
//
// =======================================================================
#pragma once

#include <string>

#include "SceneStorageConfig.h"

namespace SceneStorageYaml {

inline constexpr const char* SceneSettingsKey = "SceneSettings";
inline constexpr const char* StorageKey = "Storage";

// Scene保存RootへStorage設定を書き込む。
// Entity列とは独立したScene単位設定として保持する。
inline void EncodeIntoRoot(
	YAML::Node& root,
	const SceneStorageConfig& config
){
	SceneStorageConfig normalized = config;
	normalized.Normalize();
	root[SceneSettingsKey][StorageKey] = normalized.Encode();
}

// Scene保存RootからStorage設定だけを読み取る。
// 設定Nodeが存在しない有効な旧Sceneは既定値へ戻し、falseを返す。
// これにより、別Sceneから再読込した際に直前の設定を持ち越さない。
inline bool DecodeFromRoot(
	const YAML::Node& root,
	SceneStorageConfig& config
){
	if(!root || !root.IsMap()) return false;

	const YAML::Node settings = root[SceneSettingsKey];
	if(!settings || !settings.IsMap()){
		config = SceneStorageConfig{};
		return false;
	}

	const YAML::Node storage = settings[StorageKey];
	if(!storage || !storage.IsMap()){
		config = SceneStorageConfig{};
		return false;
	}

	SceneStorageConfig decoded{};
	decoded.Decode(storage);
	config = decoded;
	return true;
}

// Scene初期化前の先読みに使用する。
// YAML解析失敗時は呼び出し元が既定設定を維持できるようfalseを返す。
inline bool DecodeFromText(
	const std::string& yamlText,
	SceneStorageConfig& config
){
	try {
		return DecodeFromRoot(YAML::Load(yamlText), config);
	} catch(const YAML::Exception&){
		return false;
	}
}

} // namespace SceneStorageYaml
