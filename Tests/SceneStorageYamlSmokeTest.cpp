#include <cassert>
#include <string>

#include "Engine/Scene/Config/SceneStorageYaml.h"

int main(){
	SceneStorageConfig source;
	source.expectedEntityCount = 4096;
	source.expectedTransformCount = 2048;
	source.expectedRenderableCount = 1536;
	source.expectedCullingCount = 1024;
	source.expectedStaticEntityCount = 700;
	source.preallocatedTransformPages = 10;
	source.preallocatedTagPages = 4;
	source.renderPacketReserve = 1600;
	source.visibleEntityReserve = 1200;
	source.staticBatchReserve = 96;
	source.allowRuntimeGrowth = false;
	source.logCapacityGrowth = true;

	YAML::Node root;
	root["Entities"] = YAML::Node(YAML::NodeType::Sequence);
	SceneStorageYaml::EncodeIntoRoot(root, source);

	assert(root["SceneSettings"]);
	assert(root["SceneSettings"]["Storage"]);
	assert(root["Entities"]);

	SceneStorageConfig decoded;
	const bool decodedOk = SceneStorageYaml::DecodeFromRoot(root, decoded);
	assert(decodedOk);
	assert(decoded.expectedEntityCount == 4096);
	assert(decoded.expectedTransformCount == 2048);
	assert(decoded.expectedRenderableCount == 1536);
	assert(decoded.expectedCullingCount == 1024);
	assert(decoded.expectedStaticEntityCount == 700);
	assert(decoded.preallocatedTransformPages == 10);
	assert(decoded.preallocatedTagPages == 4);
	assert(decoded.renderPacketReserve == 1600);
	assert(decoded.visibleEntityReserve == 1200);
	assert(decoded.staticBatchReserve == 96);
	assert(!decoded.allowRuntimeGrowth);
	assert(decoded.logCapacityGrowth);

	SceneStorageConfig legacy;
	legacy.expectedEntityCount = 1234;
	YAML::Node legacyRoot;
	legacyRoot["Entities"] = YAML::Node(YAML::NodeType::Sequence);
	assert(!SceneStorageYaml::DecodeFromRoot(legacyRoot, legacy));
	assert(legacy.expectedEntityCount == 1234);

	SceneStorageConfig clamped;
	const std::string text =
		"SceneSettings:\n"
		"  Storage:\n"
		"    ExpectedEntities: 10\n"
		"    ExpectedTransforms: 100\n"
		"    ExpectedCullingComponents: 50\n";
	assert(SceneStorageYaml::DecodeFromText(text, clamped));
	assert(clamped.expectedEntityCount == 10);
	assert(clamped.expectedTransformCount == 10);
	assert(clamped.expectedCullingCount == 10);

	SceneStorageConfig malformed;
	malformed.expectedEntityCount = 777;
	assert(!SceneStorageYaml::DecodeFromText("SceneSettings: [", malformed));
	assert(malformed.expectedEntityCount == 777);

	return 0;
}
