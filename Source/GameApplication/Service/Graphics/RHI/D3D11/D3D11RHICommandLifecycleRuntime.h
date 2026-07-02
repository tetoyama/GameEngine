#pragma once

namespace RHI {

inline void D3D11RHICommandList::Begin(){
	m_recording = m_owner && m_owner->m_context;
	m_closed = false;
	m_renderPassActive = false;
}

inline void D3D11RHICommandList::End(){
	if(m_renderPassActive) EndRenderPass();
	m_recording = false;
	m_closed = true;
}

inline void D3D11RHICommandList::EndRenderPass(){
	if(!m_renderPassActive) return;
	if(m_owner && m_owner->m_context){
		m_owner->m_context->OMSetRenderTargets(0, nullptr, nullptr);
	}
	m_renderPassActive = false;
}

} // namespace RHI
