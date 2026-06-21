#pragma once

namespace RHI {

inline PipelineStateHandle D3D11RHIDevice::CreatePipelineState(
	const PipelineStateDesc& desc
){
	if(!m_device) return {};

	if(desc.vertexShader){
		const auto* shader = Find(desc.vertexShader);
		if(!shader || !shader->vertex) return {};
	}
	if(desc.pixelShader){
		const auto* shader = Find(desc.pixelShader);
		if(!shader || !shader->pixel) return {};
	}
	if(desc.computeShader){
		const auto* shader = Find(desc.computeShader);
		if(!shader || !shader->compute) return {};
	}

	D3D11PipelineStateResource resource;
	resource.desc = desc;
	return m_pipelines.Create(std::move(resource));
}

} // namespace RHI
