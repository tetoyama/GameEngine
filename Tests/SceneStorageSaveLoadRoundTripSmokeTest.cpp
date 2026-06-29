#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "Engine/Scene/Config/SceneStoragePreloader.h"
#include "Engine/Scene/Config/SceneStorageYaml.h"

int main(){
	SceneStorageConfig source;
	source.expectedEntityCount = 3072;
	source.expectedTransformCount = 2048;
	source.expectedRenderableCount = 1536;
	source.expectedCullingCount = 1024;
	source.expectedStaticEntityCount = 768;
	source.preallocatedTransformPages = 9;
	source.preallocatedTagPages = 3;
	source.renderPacketReserve = 1900;
	source.visibleEntityReserve = 1400;
	source.staticBatchReserve = 128;
	source.allowRuntimeGrowth = false;
	source.logCapacityGrowth = true;

	YAML::Node root;
	YAML::Node entity;
	entity["Entity"] = 42;
	entity["Components"] = YAML::Node(YAML::NodeType::Sequence);
	root["Entities"].push_back(entity);
	SceneStorageYaml::EncodeIntoRoot(root, source);

	YAML::Emitter emitter;
	emitter << root;
	assert(emitter.good());

	const std::filesystem::path path =
		std::filesystem::temp_directory_path() /
		"gameengine_scene_storage_roundtrip.yaml";
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		assert(output.is_open());
		output.write(
			emitter.c_str(),
			static_cast<std::streamsize>(emitter.size())
		);
		assert(output.good());
	}

	SceneStorageConfig preloaded;
	assert(SceneStoragePreloader::DecodeFromFile(path.string(), preloaded));
	assert(preloaded.expectedEntityCount == source.expectedEntityCount);
	assert(preloaded.expectedTransformCount == source.expectedTransformCount);
	assert(preloaded.expectedRenderableCount == source.expectedRenderableCount);
	assert(preloaded.expectedCullingCount == source.expectedCullingCount);
	assert(preloaded.expectedStaticEntityCount == source.expectedStaticEntityCount);
	assert(preloaded.preallocatedTransformPages == source.preallocatedTransformPages);
	assert(preloaded.preallocatedTagPages == source.preallocatedTagPages);
	assert(preloaded.renderPacketReserve == source.renderPacketReserve);
	assert(preloaded.visibleEntityReserve == source.visibleEntityReserve);
	assert(preloaded.staticBatchReserve == source.staticBatchReserve);
	assert(!preloaded.allowRuntimeGrowth);
	assert(preloaded.logCapacityGrowth);

	const YAML::Node reloadedRoot = YAML::LoadFile(path.string());
	assert(reloadedRoot["Entities"]);
	assert(reloadedRoot["Entities"].IsSequence());
	assert(reloadedRoot["Entities"].size() == 1);
	assert(reloadedRoot["Entities"][0]["Entity"].as<int>() == 42);

	SceneStorageConfig reloaded;
	assert(SceneStorageYaml::DecodeFromRoot(reloadedRoot, reloaded));
	assert(reloaded.staticBatchReserve == source.staticBatchReserve);
	assert(!reloaded.allowRuntimeGrowth);

	std::error_code removeError;
	std::filesystem::remove(path, removeError);
	assert(!removeError);
	return 0;
}
