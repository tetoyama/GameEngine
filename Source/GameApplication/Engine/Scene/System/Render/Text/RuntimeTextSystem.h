#pragma once

#include "Interface/ISystem.h"

#include <memory>
#include <string>
#include <wincodec.h>
#include <wrl/client.h>

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

private:
	void Update();
	bool Rasterize(
		const RuntimeTextComponent& component,
		std::shared_ptr<TextureData>& output,
		std::string* error);

	SceneManagerContext* m_context = nullptr;
	Microsoft::WRL::ComPtr<IWICImagingFactory> m_wicFactory;
	bool m_comInitializedHere = false;
};
