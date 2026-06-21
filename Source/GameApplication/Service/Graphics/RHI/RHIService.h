#pragma once

#include <memory>

#include "Service/IService.h"
#include "RHIBackend.h"

namespace RHI {

class RenderHardwareInterfaceService final : public IService {
public:
	RenderHardwareInterfaceService() = default;
	~RenderHardwareInterfaceService() override = default;

	RenderHardwareInterfaceService(const RenderHardwareInterfaceService&) = delete;
	RenderHardwareInterfaceService& operator=(const RenderHardwareInterfaceService&) = delete;

	BackendRegistry& GetRegistry() noexcept { return m_registry; }
	const BackendRegistry& GetRegistry() const noexcept { return m_registry; }

	bool SelectBackend(BackendType backendType){
		std::unique_ptr<IRHIBackend> backend = m_registry.Create(backendType);
		if(!backend || !backend->IsSupported()) return false;
		m_backend = std::move(backend);
		m_selectedBackend = backendType;
		return true;
	}

	BackendType GetSelectedBackend() const noexcept { return m_selectedBackend; }
	IRHIBackend* GetBackend() noexcept { return m_backend.get(); }
	const IRHIBackend* GetBackend() const noexcept { return m_backend.get(); }

	void Shutdown() override {
		m_backend.reset();
		m_registry.Clear();
	}

private:
	BackendRegistry m_registry;
	std::unique_ptr<IRHIBackend> m_backend;
	BackendType m_selectedBackend = BackendType::Direct3D11;
};

} // namespace RHI
