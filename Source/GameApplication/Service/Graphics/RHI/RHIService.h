#pragma once

#include <memory>

#include "Service/IService.h"
#include "RHIBackend.h"

namespace RHI {

class RHIService final : public IService {
public:
	RHIService();
	~RHIService() override;

	RHIService(const RHIService&) = delete;
	RHIService& operator=(const RHIService&) = delete;

	BackendRegistry& GetRegistry() noexcept { return m_registry; }
	const BackendRegistry& GetRegistry() const noexcept { return m_registry; }

	bool SelectBackend(BackendType backendType);

	BackendType GetSelectedBackend() const noexcept { return m_selectedBackend; }
	IRHIBackend* GetBackend() noexcept { return m_backend.get(); }
	const IRHIBackend* GetBackend() const noexcept { return m_backend.get(); }

	void Shutdown() override;

private:
	BackendRegistry m_registry;
	std::unique_ptr<IRHIBackend> m_backend;
	BackendType m_selectedBackend = BackendType::Direct3D11;
};

} // namespace RHI
