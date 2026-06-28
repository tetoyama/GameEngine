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
		m_device.reset();
		m_backend = std::move(backend);
		m_selectedBackend = backendType;
		return true;
	}

	bool AdoptDevice(std::unique_ptr<IRHIDevice> device){
		if(!device) return false;
		if(m_backend && device->GetBackendType() != m_selectedBackend){
			return false;
		}
		m_selectedBackend = device->GetBackendType();
		m_device = std::move(device);
		return true;
	}

	std::unique_ptr<IRHIDevice> ReleaseDevice() noexcept {
		return std::move(m_device);
	}

	void ResetDevice() noexcept {
		m_device.reset();
	}

	BackendType GetSelectedBackend() const noexcept { return m_selectedBackend; }
	IRHIBackend* GetBackend() noexcept { return m_backend.get(); }
	const IRHIBackend* GetBackend() const noexcept { return m_backend.get(); }
	IRHIDevice* GetDevice() noexcept { return m_device.get(); }
	const IRHIDevice* GetDevice() const noexcept { return m_device.get(); }

	void Shutdown() override {
		m_device.reset();
		m_backend.reset();
		m_registry.Clear();
	}

private:
	BackendRegistry m_registry;
	std::unique_ptr<IRHIBackend> m_backend;
	std::unique_ptr<IRHIDevice> m_device;
	BackendType m_selectedBackend = BackendType::Direct3D11;
};

} // namespace RHI
