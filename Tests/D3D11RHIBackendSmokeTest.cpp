#include <cassert>
#include <cstdint>
#include <type_traits>

#include "Service/Graphics/RHI/D3D11/D3D11RHIBackend.h"

int main(){
	static_assert(std::is_base_of_v<RHI::IRHIBackend, RHI::D3D11RHIBackend>);
	static_assert(std::is_base_of_v<RHI::IRHIDevice, RHI::D3D11RHIDevice>);

	using IndexedInstancedSignature = bool (
		RHI::D3D11RHICommandList::*
	)(uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
	static_assert(std::is_same_v<
		decltype(&RHI::D3D11RHICommandList::DrawIndexedInstanced),
		IndexedInstancedSignature
	>);

	RHI::BackendRegistry registry;
	assert(RHI::RegisterD3D11RHIBackend(registry));
	assert(registry.IsRegistered(RHI::BackendType::Direct3D11));

	auto backend = registry.Create(RHI::BackendType::Direct3D11);
	assert(backend);
	assert(backend->GetType() == RHI::BackendType::Direct3D11);
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());
	return 0;
}
