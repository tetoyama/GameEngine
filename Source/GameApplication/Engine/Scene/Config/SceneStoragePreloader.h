// =======================================================================
//
// SceneStoragePreloader.h
//
// =======================================================================
#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "SceneStorageConfig.h"
#include "SceneStorageYaml.h"

namespace SceneStoragePreloader {

// Scene本体を構築する前に、YAMLからStorage設定だけを先読みする。
// Nodeが存在しない旧Scene、ファイルを開けない場合、YAML解析失敗時は
// 呼び出し元の設定を変更せずfalseを返す。
inline bool DecodeFromFile(
	const std::string& scenePath,
	SceneStorageConfig& config
){
	if(scenePath.empty()) return false;

	std::ifstream sceneFile(scenePath, std::ios::binary);
	if(!sceneFile) return false;

	std::ostringstream yamlText;
	yamlText << sceneFile.rdbuf();
	if(sceneFile.bad()) return false;

	SceneStorageConfig decoded = config;
	if(!SceneStorageYaml::DecodeFromText(yamlText.str(), decoded)){
		return false;
	}

	config = decoded;
	return true;
}

} // namespace SceneStoragePreloader
