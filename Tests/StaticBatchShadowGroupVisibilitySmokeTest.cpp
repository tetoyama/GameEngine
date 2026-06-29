#include <array>
#include <cassert>
#include <cstddef>

#include "Engine/Scene/System/Render/StaticBatch/StaticBatchVisibleInstanceBuffer.h"

namespace {

StaticBatchInstanceData MakeInstance(std::uint32_t entityIndex){
	StaticBatchInstanceData instance;
	instance.entityIndex = entityIndex;
	instance.entityGeneration = 1;
	instance.sceneContextID = 7;
	return instance;
}

} // namespace

int main(){
	StaticBatchInstanceGroup sourceGroup;
	sourceGroup.key.kind = RenderPacketKind::Model;
	sourceGroup.key.layer = RenderLayer::Opaque3D;
	sourceGroup.sceneContextID = 7;
	sourceGroup.representativePacketIndex = 10;
	sourceGroup.firstInstance = 0;
	sourceGroup.instanceCount = 4;

	const std::array<StaticBatchInstanceGroup, 1> sourceGroups{
		sourceGroup
	};
	const std::array<StaticBatchInstanceData, 4> sourceInstances{
		MakeInstance(100),
		MakeInstance(101),
		MakeInstance(102),
		MakeInstance(103)
	};
	constexpr std::array<std::size_t, 4> sourcePacketIndices{
		10, 11, 12, 13
	};

	StaticBatchVisibleInstanceBuffer visible;
	visible.Reserve(sourceGroups.size(), sourceInstances.size());
	assert(visible.Build(
		sourceGroups,
		sourceInstances,
		sourcePacketIndices,
		1000,
		2000,
		false,
		[](const StaticBatchInstanceData&, std::size_t sourceIndex){
			return sourceIndex == 0 || sourceIndex >= 2;
		}
	));
	assert(visible.IsValid());
	assert(!visible.IsOverflowed());
	assert(visible.SourceRevision() == 1000);
	assert(visible.ViewRevision() == 2000);
	assert(visible.SelectionRevision() != 0);
	assert(visible.Groups().size() == 1);
	assert(visible.SourceGroupIndices().size() == 1);
	assert(visible.SourceGroupIndices()[0] == 0);
	assert(visible.Groups()[0].representativePacketIndex == 10);
	assert(visible.Groups()[0].firstInstance == 0);
	assert(visible.Groups()[0].instanceCount == 3);
	assert(visible.Instances().size() == 3);
	assert(visible.Instances()[0].entityIndex == 100);
	assert(visible.Instances()[1].entityIndex == 102);
	assert(visible.Instances()[2].entityIndex == 103);
	assert(visible.PacketIndices().size() == 3);
	assert(visible.PacketIndices()[0] == 10);
	assert(visible.PacketIndices()[1] == 12);
	assert(visible.PacketIndices()[2] == 13);

	const auto mixedTelemetry = visible.Telemetry();
	assert(mixedTelemetry.sourceGroupCount == 1);
	assert(mixedTelemetry.outputGroupCount == 1);
	assert(mixedTelemetry.visibleInstanceCount == 3);
	assert(mixedTelemetry.culledInstanceCount == 1);
	assert(mixedTelemetry.mixedGroupCount == 1);
	assert(mixedTelemetry.allVisibleGroupCount == 0);
	assert(mixedTelemetry.allCulledGroupCount == 0);

	std::size_t cachedPredicateCalls = 0;
	const std::uint64_t cachedSelectionRevision = visible.SelectionRevision();
	assert(visible.Build(
		sourceGroups,
		sourceInstances,
		sourcePacketIndices,
		1000,
		2000,
		false,
		[&](const StaticBatchInstanceData&, std::size_t){
			++cachedPredicateCalls;
			return false;
		}
	));
	assert(cachedPredicateCalls == 0);
	assert(visible.SelectionRevision() == cachedSelectionRevision);

	assert(visible.Build(
		sourceGroups,
		sourceInstances,
		sourcePacketIndices,
		1000,
		2001,
		false,
		[](const StaticBatchInstanceData&, std::size_t sourceIndex){
			return sourceIndex == 1;
		}
	));
	assert(visible.Groups().size() == 1);
	assert(visible.Groups()[0].representativePacketIndex == 11);
	assert(visible.Groups()[0].instanceCount == 1);
	assert(visible.Instances().size() == 1);
	assert(visible.Instances()[0].entityIndex == 101);
	assert(visible.PacketIndices()[0] == 11);

	assert(visible.Build(
		sourceGroups,
		sourceInstances,
		sourcePacketIndices,
		1000,
		2002,
		false,
		[](const StaticBatchInstanceData&, std::size_t){
			return false;
		}
	));
	assert(visible.Groups().empty());
	assert(visible.Instances().empty());
	assert(visible.PacketIndices().empty());
	assert(visible.Telemetry().allCulledGroupCount == 1);

	StaticBatchVisibleInstanceBuffer constrained;
	assert(!constrained.Build(
		sourceGroups,
		sourceInstances,
		sourcePacketIndices,
		1000,
		2000,
		false,
		[](const StaticBatchInstanceData&, std::size_t){
			return true;
		}
	));
	assert(!constrained.IsValid());
	assert(constrained.IsOverflowed());
	assert(constrained.Groups().empty());
	assert(constrained.Instances().empty());
	return 0;
}
