from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


render_packet = r'''#pragma once

#include <algorithm>
#include <cstdint>

#include "Scene/Entity/Entity.h"
#include "System/Render/RenderSystem/renderLayer.h"

enum class RenderPacketKind : uint8_t {
	Model = 0,
	Mesh,
	Sprite,
	Billboard,
	Particle,
	Terrain,
	Wave,
	Effect
};

enum class RenderPacketPassMask : uint8_t {
	None = 0,
	Shadow = 1u << 0,
	GBuffer = 1u << 1,
	Forward = 1u << 2,
	Overlay = 1u << 3,
	Debug = 1u << 4
};

constexpr RenderPacketPassMask operator|(
	RenderPacketPassMask lhs,
	RenderPacketPassMask rhs
) noexcept {
	return static_cast<RenderPacketPassMask>(
		static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
	);
}

constexpr bool HasRenderPacketPass(
	RenderPacketPassMask value,
	RenderPacketPassMask pass
) noexcept {
	return (static_cast<uint8_t>(value) & static_cast<uint8_t>(pass)) != 0;
}

constexpr RenderPacketPassMask RenderPacketPassesForLayer(
	RenderLayer layer
) noexcept {
	switch(layer){
		case RenderLayer::Opaque3D:
			return RenderPacketPassMask::Shadow | RenderPacketPassMask::GBuffer;
		case RenderLayer::Background2D:
		case RenderLayer::Transparent3D:
		case RenderLayer::SortTransparent3D:
			return RenderPacketPassMask::Forward;
		case RenderLayer::OverlayUI:
			return RenderPacketPassMask::Overlay;
		case RenderLayer::Debug:
			return RenderPacketPassMask::Debug;
		default:
			return RenderPacketPassMask::None;
	}
}

struct RenderPacketTransformSnapshot {
	float position[3] = {0.0f, 0.0f, 0.0f};
	float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	float scale[3] = {1.0f, 1.0f, 1.0f};
};

constexpr uint16_t EncodeRenderPacketOrder(int32_t order) noexcept {
	const int32_t clamped = (std::max)(-32768, (std::min)(32767, order));
	return static_cast<uint16_t>(clamped + 32768);
}

// Primary sort key layout:
// [63:60] RenderLayer, [59:56] PacketKind, [55:40] signed OrderInLayer,
// [39:08] Material key, [07:00] reserved.
// Scene / Entity / generation are deterministic tie breakers and are intentionally
// kept outside this key so the material grouping contract remains visible.
constexpr uint64_t MakeRenderPacketSortKey(
	RenderLayer layer,
	RenderPacketKind kind,
	uint32_t materialKey,
	int32_t orderInLayer
) noexcept {
	return
		(static_cast<uint64_t>(static_cast<uint8_t>(layer) & 0x0fu) << 60) |
		(static_cast<uint64_t>(static_cast<uint8_t>(kind) & 0x0fu) << 56) |
		(static_cast<uint64_t>(EncodeRenderPacketOrder(orderInLayer)) << 40) |
		(static_cast<uint64_t>(materialKey) << 8);
}

struct RenderPacket {
	uint32_t sceneContextID = 0;
	Entity entity{};
	RenderPacketKind kind = RenderPacketKind::Model;
	RenderLayer layer = RenderLayer::Opaque3D;
	RenderPacketPassMask passMask = RenderPacketPassMask::None;
	uint32_t materialKey = 0;
	int32_t orderInLayer = 0;
	uint64_t sortKey = 0;
	uint64_t stableSequence = 0;
	RenderPacketTransformSnapshot transform;
};

inline bool RenderPacketLess(const RenderPacket& lhs, const RenderPacket& rhs) noexcept {
	if(lhs.sortKey != rhs.sortKey) return lhs.sortKey < rhs.sortKey;
	if(lhs.sceneContextID != rhs.sceneContextID) return lhs.sceneContextID < rhs.sceneContextID;
	if(lhs.entity.GetPackedValue() != rhs.entity.GetPackedValue()){
		return lhs.entity.GetPackedValue() < rhs.entity.GetPackedValue();
	}
	if(lhs.kind != rhs.kind){
		return static_cast<uint8_t>(lhs.kind) < static_cast<uint8_t>(rhs.kind);
	}
	return lhs.stableSequence < rhs.stableSequence;
}
'''
save(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacket.h",
    render_packet,
)


render_packet_buffer = r'''#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "RenderPacket.h"

class RenderPacketWorkerBuffer {
public:
	explicit RenderPacketWorkerBuffer(uint32_t workerIndex = 0) noexcept
		: m_workerIndex(workerIndex) {
	}

	uint32_t WorkerIndex() const noexcept { return m_workerIndex; }

	void Clear(){ m_packets.clear(); }

	void Reserve(size_t count){ m_packets.reserve(count); }

	void Add(RenderPacket packet){
		m_packets.emplace_back(std::move(packet));
	}

	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }

private:
	uint32_t m_workerIndex = 0;
	std::vector<RenderPacket> m_packets;
};

// Frame-local CPU packet storage. Scheduler resource Write/Read hazards guarantee
// that Merge completes before MainThread submission reads the published packets.
class RenderPacketFrameBuffer {
public:
	void BeginFrame(uint64_t generation){
		m_generation = generation;
		m_packets.clear();
		m_ready = false;
	}

	void Merge(std::span<const RenderPacketWorkerBuffer> workerBuffers){
		std::vector<const RenderPacketWorkerBuffer*> orderedWorkers;
		orderedWorkers.reserve(workerBuffers.size());
		for(const RenderPacketWorkerBuffer& buffer : workerBuffers){
			orderedWorkers.push_back(&buffer);
		}
		std::sort(
			orderedWorkers.begin(),
			orderedWorkers.end(),
			[](const auto* lhs, const auto* rhs){
				return lhs->WorkerIndex() < rhs->WorkerIndex();
			}
		);

		size_t packetCount = 0;
		for(const auto* worker : orderedWorkers){
			packetCount += worker->Packets().size();
		}
		m_packets.reserve(packetCount);

		for(const auto* worker : orderedWorkers){
			m_packets.insert(
				m_packets.end(),
				worker->Packets().begin(),
				worker->Packets().end()
			);
		}

		std::stable_sort(m_packets.begin(), m_packets.end(), RenderPacketLess);
		m_ready = true;
	}

	uint64_t Generation() const noexcept { return m_generation; }
	bool IsReady() const noexcept { return m_ready; }
	const std::vector<RenderPacket>& Packets() const noexcept { return m_packets; }
	size_t Size() const noexcept { return m_packets.size(); }

	size_t Count(RenderPacketKind kind) const noexcept {
		return static_cast<size_t>(std::count_if(
			m_packets.begin(),
			m_packets.end(),
			[kind](const RenderPacket& packet){ return packet.kind == kind; }
		));
	}

private:
	uint64_t m_generation = 0;
	bool m_ready = false;
	std::vector<RenderPacket> m_packets;
};
'''
save(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h",
    render_packet_buffer,
)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.h"
text = load(path)
text = replace_once(
    text,
    '#include "System/Render/RenderSystem/renderLayer.h"\n',
    '#include "System/Render/RenderSystem/renderLayer.h"\n'
    '#include "System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"\n',
    "Render Packet buffer include",
)
text = replace_once(
    text,
    '''\tvoid Draw();

\tvoid RegisterTasks(SystemScheduleBuilder& builder) override;
''',
    '''\tvoid Draw();
\tvoid BuildRenderPackets();
\tvoid SubmitRenderPackets();

\tvoid RegisterTasks(SystemScheduleBuilder& builder) override;
''',
    "Render Packet methods",
)
text = replace_once(
    text,
    '''\tID3D11SamplerState* GetEnvMapSampler() const;

\tstd::vector<ShaderMaterial> ShaderMaterials;
''',
    '''\tID3D11SamplerState* GetEnvMapSampler() const;

\tconst RenderPacketFrameBuffer& GetRenderPacketBuffer() const noexcept {
\t\treturn m_renderPacketBuffer;
\t}

\tuint64_t GetLastSubmittedPacketGeneration() const noexcept {
\t\treturn m_lastSubmittedPacketGeneration;
\t}

\tstd::vector<ShaderMaterial> ShaderMaterials;
''',
    "Render Packet accessors",
)
text = replace_once(
    text,
    '''\tstd::shared_ptr<PixelShaderData> ForwardPSDebug;

\tfloat lazyTimer = 0.0f;
''',
    '''\tstd::shared_ptr<PixelShaderData> ForwardPSDebug;

\tRenderPacketFrameBuffer m_renderPacketBuffer;
\tuint64_t m_renderPacketGeneration = 0;
\tuint64_t m_lastSubmittedPacketGeneration = 0;

\tfloat lazyTimer = 0.0f;
''',
    "Render Packet members",
)
save(path, text)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(path)
text = replace_once(
    text,
    '#include <algorithm>\n#include <filesystem>\n#include <queue>\n#include <thread>\n',
    '#include <algorithm>\n#include <array>\n#include <filesystem>\n#include <queue>\n#include <thread>\n',
    "array include",
)
text = replace_once(
    text,
    '#include <Component/modelRendererComponent.h>\n#include <Component/LightComponent.h>\n#include <Component/textureComponent.h>\n#include <Component/environmentMapComponent.h>\n',
    '#include <Component/modelRendererComponent.h>\n'
    '#include <Component/meshRendererComponent.h>\n'
    '#include <Component/BillBoardRendererComponent.h>\n'
    '#include <Component/2DspriteRendererComponent.h>\n'
    '#include <Component/terrainComponent.h>\n'
    '#include <Component/waveComponent.h>\n'
    '#include <Component/particleComponent.h>\n'
    '#include <Component/EffectComponent.h>\n'
    '#include <Component/LightComponent.h>\n'
    '#include <Component/textureComponent.h>\n'
    '#include <Component/environmentMapComponent.h>\n',
    "Renderable component includes",
)

insert_before_register = r'''
void RenderSystem::BuildRenderPackets(){
	std::array<RenderPacketWorkerBuffer, 1> workerBuffers{
		RenderPacketWorkerBuffer(0)
	};
	RenderPacketWorkerBuffer& worker = workerBuffers[0];
	uint64_t stableSequence = 0;

	struct SceneEntry {
		uint32_t contextID = 0;
		std::string name;
		SceneContext* context = nullptr;
	};

	std::vector<SceneEntry> scenes;
	for(const auto& [sceneName, scene] : m_context->sceneManager->GetActiveScenes()){
		if(!scene) continue;
		SceneContext* context = scene->GetSceneContext();
		if(!context || !context->component || context->contextID == 0) continue;
		scenes.push_back({context->contextID, sceneName, context});
	}
	std::sort(
		scenes.begin(),
		scenes.end(),
		[](const SceneEntry& lhs, const SceneEntry& rhs){
			if(lhs.contextID != rhs.contextID) return lhs.contextID < rhs.contextID;
			return lhs.name < rhs.name;
		}
	);

	for(const SceneEntry& sceneEntry : scenes){
		ComponentRegistry* components = sceneEntry.context->component;
		auto entities = components->FindEntitiesWithComponent<TransformComponent>();
		std::sort(
			entities.begin(),
			entities.end(),
			[](Entity lhs, Entity rhs){
				return lhs.GetPackedValue() < rhs.GetPackedValue();
			}
		);

		worker.Reserve(worker.Packets().size() + entities.size());
		for(Entity entity : entities){
			const TransformComponent* transform =
				components->GetComponent<TransformComponent>(entity);
			if(!transform) continue;

			const RenderLayerComponent* layerComponent =
				components->GetComponent<RenderLayerComponent>(entity);
			const OrderInLayerComponent* orderComponent =
				components->GetComponent<OrderInLayerComponent>(entity);
			const MaterialComponent* materialComponent =
				components->GetComponent<MaterialComponent>(entity);

			const RenderLayer layer = layerComponent
				? layerComponent->layer
				: RenderLayer::Opaque3D;
			const int32_t orderInLayer = orderComponent
				? orderComponent->order
				: 0;
			const uint32_t materialKey = materialComponent
				? static_cast<uint32_t>((std::max)(0, materialComponent->ShaderID))
				: 0u;

			RenderPacketTransformSnapshot snapshot;
			snapshot.position[0] = transform->position.x;
			snapshot.position[1] = transform->position.y;
			snapshot.position[2] = transform->position.z;
			const DirectX::XMFLOAT4& rotation = transform->GetRotation();
			snapshot.rotation[0] = rotation.x;
			snapshot.rotation[1] = rotation.y;
			snapshot.rotation[2] = rotation.z;
			snapshot.rotation[3] = rotation.w;
			snapshot.scale[0] = transform->scale.x;
			snapshot.scale[1] = transform->scale.y;
			snapshot.scale[2] = transform->scale.z;

			auto appendPacket = [&](RenderPacketKind kind){
				RenderPacket packet;
				packet.sceneContextID = sceneEntry.contextID;
				packet.entity = entity;
				packet.kind = kind;
				packet.layer = layer;
				packet.passMask = RenderPacketPassesForLayer(layer);
				packet.materialKey = materialKey;
				packet.orderInLayer = orderInLayer;
				packet.sortKey = MakeRenderPacketSortKey(
					layer,
					kind,
					materialKey,
					orderInLayer
				);
				packet.stableSequence = stableSequence++;
				packet.transform = snapshot;
				worker.Add(std::move(packet));
			};

			if(components->GetComponent<ModelRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Model);
			}
			if(components->GetComponent<MeshRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Mesh);
			}
			if(components->GetComponent<SpriteRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Sprite);
			}
			if(components->GetComponent<BillBoardRendererComponent>(entity)){
				appendPacket(RenderPacketKind::Billboard);
			}
			if(components->GetComponent<ParticleComponent>(entity)){
				appendPacket(RenderPacketKind::Particle);
			}
			if(components->GetComponent<TerrainComponent>(entity)){
				appendPacket(RenderPacketKind::Terrain);
			}
			if(components->GetComponent<WaveComponent>(entity)){
				appendPacket(RenderPacketKind::Wave);
			}
			if(components->GetComponent<EffectComponent>(entity)){
				appendPacket(RenderPacketKind::Effect);
			}
		}
	}

	m_renderPacketBuffer.BeginFrame(++m_renderPacketGeneration);
	m_renderPacketBuffer.Merge(workerBuffers);
}

void RenderSystem::SubmitRenderPackets(){
	m_lastSubmittedPacketGeneration = m_renderPacketBuffer.Generation();
	Draw();
}

'''
text = replace_once(
    text,
    'void RenderSystem::RegisterTasks(SystemScheduleBuilder& builder){\n',
    insert_before_register + 'void RenderSystem::RegisterTasks(SystemScheduleBuilder& builder){\n',
    "Render Packet build and submit methods",
)

old_register_tail = '''\tbuilder.AddTask(
\t\t"RenderSystem.Frame.Submit",
\t\tSystemTaskDomain::Render,
\t\tSystemPhase::Late,
\t\t0,
\t\tSystemAccess::LegacyExclusive(),
\t\tThreadAffinity::MainThread,
\t\t[this](const SystemTaskContext&){
\t\t\tDraw();
\t\t}
\t);
'''
new_register_tail = '''\tSystemAccess packetBuildAccess;
\tpacketBuildAccess
\t\t.ReadComponent<TransformComponent>()
\t\t.ReadComponent<RenderLayerComponent>()
\t\t.ReadComponent<OrderInLayerComponent>()
\t\t.ReadComponent<MaterialComponent>()
\t\t.ReadComponent<ModelRendererComponent>()
\t\t.ReadComponent<MeshRendererComponent>()
\t\t.ReadComponent<SpriteRendererComponent>()
\t\t.ReadComponent<BillBoardRendererComponent>()
\t\t.ReadComponent<ParticleComponent>()
\t\t.ReadComponent<TerrainComponent>()
\t\t.ReadComponent<WaveComponent>()
\t\t.ReadComponent<EffectComponent>()
\t\t.ReadResource<SceneManager>()
\t\t.WriteResource<RenderPacketFrameBuffer>();

\tbuilder.AddTask(
\t\t"RenderSystem.Packet.Build",
\t\tSystemTaskDomain::Render,
\t\tSystemPhase::Early,
\t\t0,
\t\tstd::move(packetBuildAccess),
\t\tThreadAffinity::AnyWorker,
\t\t[this](const SystemTaskContext&){
\t\t\tBuildRenderPackets();
\t\t}
\t);

\tSystemAccess submitAccess = SystemAccess::LegacyExclusive();
\tsubmitAccess.ReadResource<RenderPacketFrameBuffer>();
\tbuilder.AddTask(
\t\t"RenderSystem.Command.Submit",
\t\tSystemTaskDomain::Render,
\t\tSystemPhase::Late,
\t\t0,
\t\tstd::move(submitAccess),
\t\tThreadAffinity::MainThread,
\t\t[this](const SystemTaskContext&){
\t\t\tSubmitRenderPackets();
\t\t}
\t);
'''
text = replace_once(
    text,
    old_register_tail,
    new_register_tail,
    "Render Packet tasks",
)
save(path, text)


test = r'''#include <array>
#include <cassert>
#include <vector>

#include "Engine/Scene/System/Render/RenderSystem/RenderPacket/RenderPacketBuffer.h"

namespace {

RenderPacket MakePacket(
	uint32_t sceneID,
	Entity entity,
	RenderPacketKind kind,
	RenderLayer layer,
	uint32_t material,
	int32_t order,
	uint64_t sequence
){
	RenderPacket packet;
	packet.sceneContextID = sceneID;
	packet.entity = entity;
	packet.kind = kind;
	packet.layer = layer;
	packet.passMask = RenderPacketPassesForLayer(layer);
	packet.materialKey = material;
	packet.orderInLayer = order;
	packet.sortKey = MakeRenderPacketSortKey(layer, kind, material, order);
	packet.stableSequence = sequence;
	return packet;
}

std::vector<uint64_t> PacketIDs(const RenderPacketFrameBuffer& frame){
	std::vector<uint64_t> values;
	for(const RenderPacket& packet : frame.Packets()){
		values.push_back(
			(static_cast<uint64_t>(packet.sceneContextID) << 48) ^
			packet.entity.GetPackedValue() ^
			(static_cast<uint64_t>(packet.kind) << 40)
		);
	}
	return values;
}

} // namespace

int main(){
	static_assert(!HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::OverlayUI),
		RenderPacketPassMask::GBuffer
	));
	static_assert(HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::Opaque3D),
		RenderPacketPassMask::Shadow
	));
	static_assert(HasRenderPacketPass(
		RenderPacketPassesForLayer(RenderLayer::Opaque3D),
		RenderPacketPassMask::GBuffer
	));

	RenderPacketWorkerBuffer worker0(0);
	RenderPacketWorkerBuffer worker1(1);

	worker1.Add(MakePacket(2, Entity(7, 1), RenderPacketKind::Wave,
		RenderLayer::Transparent3D, 5, 0, 0));
	worker0.Add(MakePacket(1, Entity(9, 0), RenderPacketKind::Model,
		RenderLayer::Opaque3D, 2, 0, 0));
	worker0.Add(MakePacket(1, Entity(3, 0), RenderPacketKind::Model,
		RenderLayer::Opaque3D, 1, 0, 1));
	worker1.Add(MakePacket(1, Entity(4, 0), RenderPacketKind::Sprite,
		RenderLayer::OverlayUI, 0, -5, 1));

	std::array<RenderPacketWorkerBuffer, 2> reversedWorkers{worker1, worker0};
	RenderPacketFrameBuffer frameA;
	frameA.BeginFrame(11);
	frameA.Merge(reversedWorkers);

	std::array<RenderPacketWorkerBuffer, 2> orderedWorkers{worker0, worker1};
	RenderPacketFrameBuffer frameB;
	frameB.BeginFrame(11);
	frameB.Merge(orderedWorkers);

	assert(frameA.IsReady());
	assert(frameA.Generation() == 11);
	assert(frameA.Size() == 4);
	assert(frameA.Count(RenderPacketKind::Model) == 2);
	assert(PacketIDs(frameA) == PacketIDs(frameB));

	const auto& packets = frameA.Packets();
	for(size_t index = 1; index < packets.size(); ++index){
		assert(!RenderPacketLess(packets[index], packets[index - 1]));
	}
	assert(packets[0].entity == Entity(3, 0));
	assert(packets[1].entity == Entity(9, 0));
	assert(packets[2].kind == RenderPacketKind::Wave);
	assert(packets[3].kind == RenderPacketKind::Sprite);
	return 0;
}
'''
save("Tests/RenderPacketBufferSmokeTest.cpp", test)


path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(
    text,
    "- [ ] 既存Player View実機回帰\n",
    "- [x] 既存Player View実機回帰\n",
    "Player View real-device regression completion",
)
text = replace_once(
    text,
    "- `Docs/Step16G_Player_View_Regression_Guard.md`\n",
    "- `Docs/Step16G_Player_View_Regression_Guard.md`\n"
    "- `Docs/Step16G_Player_View_Regression_Completion.md`\n",
    "Player View completion document link",
)
text = replace_once(
    text,
    '''1. 既存Player View実機回帰
2. Step 17-B Render Packet基盤
3. Step 17-C Animation CPU Build / GPU Upload分離
4. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
5. Step 17-E Wave CPU Vertex Build / GPU Upload分離
''',
    '''1. Step 17-B Render Packet既存RenderPass接続
2. Step 17-C Animation CPU Build / GPU Upload分離
3. Step 17-D Terrain CPU Mesh Build / GPU Upload分離
4. Step 17-E Wave CPU Vertex Build / GPU Upload分離
''',
    "Current position after Player View verification",
)
save(path, text)


path = "Docs/Step17_Task_Naming_And_Render_Task_Decomposition_Plan.md"
text = load(path)
text = replace_once(
    text,
    "状態: **未着手**\n\n### 目的\n\n`RenderSystem.Frame.Submit`からECS走査と描画準備を分離し",
    "状態: **基盤実装中**\n\n### 目的\n\n`RenderSystem.Command.Submit`からECS走査と描画準備を分離し",
    "Step 17-B state and task name",
)
text = replace_once(text, "- [ ] API非依存`RenderPacket`定義\n", "- [x] API非依存`RenderPacket`定義\n", "RenderPacket definition checklist")
text = replace_once(text, "- [ ] Frame-local Packet Buffer\n", "- [x] Frame-local Packet Buffer\n", "Frame-local packet checklist")
text = replace_once(text, "- [ ] Packet Sort Key仕様\n", "- [x] Packet Sort Key仕様\n", "Sort key checklist")
text = replace_once(text, "- [ ] Worker-local Packet Bufferと決定的Merge\n", "- [x] Worker-local Packet Bufferと決定的Merge\n", "Deterministic merge checklist")
text = replace_once(text, "- [ ] MainThread Submit Task\n", "- [x] MainThread Submit Task\n", "Submit task checklist")
text = replace_once(
    text,
    "- `Docs/Step17A_Task_Naming_Completion.md`\n",
    "- `Docs/Step17A_Task_Naming_Completion.md`\n"
    "- `Docs/Step17B_Render_Packet_Foundation.md`\n",
    "Render Packet foundation document link",
)
save(path, text)


player_completion = '''# Step 16-G: Player View Regression Completion

## 実機確認結果

2026-06-23、既存Player Viewの実機回帰確認を完了した。

確認済み項目:

- Editor ViewとPlayer Viewの同時表示・更新
- Player View Resize後の描画領域とアスペクト比
- Play / Pause / Stop / Step遷移後の映像維持
- PostEffectとOverlay UIのPlayer View表示
- Player Viewを閉じたPlaying状態でのMain Window描画

自動化済みの回帰Guard、D3D11実描画Triangle Test、Debug / Release x64回帰と合わせ、Step 16の検証項目を完了とする。
'''
save("Docs/Step16G_Player_View_Regression_Completion.md", player_completion)


foundation_doc = '''# Step 17-B: Render Packet Foundation

## 今回の実装範囲

既存RenderPassの描画結果を変更せず、Render Packet分離の基盤を実行Scheduleへ追加した。

- API非依存`RenderPacket`
- Transform Snapshot
- RenderLayerからPass Maskへの分類
- Material / Orderを含む64-bit Sort Key
- Worker-local Packet Buffer
- Worker Index順Mergeと決定的Stable Sort
- Frame Generationを持つFrame-local Packet Buffer
- `RenderSystem.Packet.Build` AnyWorker Task
- `RenderSystem.Command.Submit` MainThread Task
- Packet BufferのResource Write / Read依存

## Packet Build対象

- Model
- Mesh
- Sprite
- Billboard
- Particle
- Terrain
- Wave
- Effect

## 現在の互換経路

`RenderSystem.Command.Submit`は完成済みPacketのGenerationを確認した後、既存`Draw()`を呼ぶ。既存RenderPassはまだComponentRegistryを直接走査するため、描画結果を変えずにPacket生成コストとSchedule分離を先に観測できる。

次の小工程でGBuffer / Shadow / Forward / Overlayの順にPacket入力へ接続し、MainThread Submit中のECS再走査を除去する。

## Sort Key仕様

```text
[63:60] RenderLayer
[59:56] RenderPacketKind
[55:40] signed OrderInLayer
[39:08] Material Key
[07:00] reserved
```

Scene Context ID、Entity index / generation、Packet kind、stable sequenceをTie Breakerとして使用する。カメラ依存の透明距離SortはView Packet構築時に別段階で適用する。

## 検証

`RenderPacketBufferSmokeTest`で次を確認する。

- Worker Buffer入力順が異なってもMerge結果が一致する
- Sort Key順とTie Breaker順が安定する
- Frame GenerationとReady状態が更新される
- Packet Kind集計が一致する
- LayerからPass Maskへの分類が正しい
'''
save("Docs/Step17B_Render_Packet_Foundation.md", foundation_doc)

print("Render Packet foundation migration applied")
