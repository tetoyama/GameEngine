#pragma once

#include "Interface/ISystem.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <wincodec.h>
#include <wrl/client.h>

// Windows SDK headers can expose function-like min/max macros. Remove them
// before RuntimeTextSystem.cpp uses the standard library algorithms.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

struct SceneManagerContext;
class RuntimeTextComponent;
struct TextureData;

class RuntimeTextSystem final : public ISystem {
public:
	explicit RuntimeTextSystem(SceneManagerContext* context)
		: m_context(context){}

	const char* GetSystemName() const override{ return "RuntimeTextSystem"; }
	void Initialize() override;
	void Finalize() override;
	void RegisterTasks(SystemScheduleBuilder& builder) override;

	// ElemenTactics owns this facade locally until it becomes a globally registered Engine system.
	// Must be called on the main thread before sprite packet submission.
	void ProcessDirtyText();

private:
	enum class MotionKind : std::uint8_t {
		None,
		SoftPop,
		BoardPulse,
		PiecePop,
		CardDeal,
		TitleReveal,
		StatusImpact,
		StatusScout,
		StatusMove,
		GameSet,
		Winner
	};

	enum class CueKind : std::uint8_t {
		None,
		Battle,
		Scout,
		Move,
		Reorder,
		GameSet
	};

	struct MotionState {
		float basePositionX = 0.0f;
		float basePositionY = 0.0f;
		float baseScaleX = 1.0f;
		float baseScaleY = 1.0f;
		float delaySeconds = 0.0f;
		float durationSeconds = 0.2f;
		MotionKind kind = MotionKind::None;
		std::chrono::steady_clock::time_point started{};
	};

	bool Rasterize(
		const RuntimeTextComponent& component,
		std::shared_ptr<TextureData>& output,
		std::string* error);

	SceneManagerContext* m_context = nullptr;
	Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
	std::unordered_map<std::uint64_t, MotionState> m_motionStates;
	std::uint64_t m_lastCueEntity = 0;
	CueKind m_cueKind = CueKind::None;
	std::chrono::steady_clock::time_point m_cueStarted{};
	bool m_comInitializedHere = false;
};
