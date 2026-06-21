#include "RHIService.h"

#include "D3D11/D3D11RHIBackend.h"
#include "Null/NullRHIBackend.h"

namespace RHI {

RHIService::RHIService(){
	RegisterNullRHIBackend(m_registry);
	RegisterD3D11RHIBackend(m_registry);
}

RHIService::~RHIService(){
	Shutdown();
}

bool RHIService::SelectBackend(BackendType backendType){
	std::unique_ptr<IRHIBackend> backend = m_registry.Create(backendType);
	if(!backend || !backend->IsSupported()) return false;

	m_backend = std::move(backend);
	m_selectedBackend = backendType;
	return true;
}

void RHIService::Shutdown(){
	m_backend.reset();
	m_registry.Clear();
}

} // namespace RHI
