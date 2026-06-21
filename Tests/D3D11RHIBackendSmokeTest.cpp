#include <cassert>
#include <memory>
#include <type_traits>

#include "Service/Graphics/RHI/D3D11/D3D11RHIBackend.h"

int main(){
	static_assert(std::is_base_of_v<RHI::IRHIBackend, RHI::D3D11RHIBackend>);
	static_assert(std::is_base_of_v<RHI::IRHIDevice, RHI::D3D11RHIDevice>);
	static_assert(std::is_base_of_v<RHI::IRHICommandQueue, RHI::D3D11RHICommandQueue>);
	static_assert(std::is_base_of_v<RHI::IRHICommandList, RHI::D3D11RHICommandList>);
	static_assert(std::is_base_of_v<RHI::IRHIFence, RHI::D3D11RHIFence>);

	const bool registered = RHI::RegisterD3D11RHIBackend();
	assert(registered || RHI::BackendRegistry::Instance().IsRegistered(
		RHI::BackendType::Direct3D11
	));

	std::unique_ptr<RHI::IRHIBackend> backend =
		RHI::BackendRegistry::Instance().Create(
			RHI::BackendType::Direct3D11
		);
	assert(backend);
	assert(backend->GetType() == RHI::BackendType::Direct3D11);
	assert(backend->GetName() == "Direct3D 11");
	assert(backend->IsSupported());
	assert(!backend->EnumerateAdapters().empty());

	return 0;
}
