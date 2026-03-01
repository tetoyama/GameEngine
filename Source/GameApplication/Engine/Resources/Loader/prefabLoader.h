// prefabLoader.h
#pragma once
#include "ResourceLoader.h"
#include "Resources/Data/prefabData.h"

#include <fstream>
#include <backends/yaml-cpp/yaml.h>

// プレファブファイル（YAML）を読み込み、PrefabData としてキャッシュする
template<>
inline void ResourceLoader<PrefabData>::SetupLoadFunc(void* /*contextPtr*/) {
	SetLoadFunction([](const std::string& path, std::shared_ptr<void>) -> std::shared_ptr<PrefabData> {
		std::ifstream fin(path);
		if (!fin.is_open()) return nullptr;

		auto data = std::make_shared<PrefabData>();
		data->filePath = path;
		data->root = YAML::Load(fin);

		// "Entities" シーケンスが存在しない場合は無効なプレファブとみなす
		if (!data->root["Entities"] || !data->root["Entities"].IsSequence()) return nullptr;

		return data;
	});
}
