#pragma once

#include "Interface/ISystem.h"

#include <memory>
#include <string>
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
	bool Rasterize(
		const RuntimeTextComponent& component,
		std::shared_ptr<TextureData>& output,
		std::string* error);

	SceneManagerContext* m_context = nullptr;
	Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
	bool m_comInitializedHere = false;
};
