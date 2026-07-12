// =======================================================================
// 
// mainRenderer.cpp
// 
// =======================================================================
#include "MainRenderer.h"
#include <dwrite.h>
#include <d2d1.h>
#include <utility>
#include <vector>

void MainRenderer::DrawText2D(
	const std::wstring& text,
	float x,
	float y,
	float fontSize,
	D2D1::ColorF color
){
	if(text.empty()){
		return;
	}

	Runtime2DCommand command;
	command.type = Runtime2DCommandType::Text;
	command.text = text;
	command.x = x;
	command.y = y;
	command.fontSize = fontSize;
	command.color = color;
	m_runtime2DCommands.push_back(std::move(command));
}

void MainRenderer::FillRect2D(
	float x,
	float y,
	float width,
	float height,
	D2D1::ColorF color
){
	if(width <= 0.0f || height <= 0.0f){
		return;
	}

	Runtime2DCommand command;
	command.type = Runtime2DCommandType::FillRect;
	command.x = x;
	command.y = y;
	command.width = width;
	command.height = height;
	command.color = color;
	m_runtime2DCommands.push_back(std::move(command));
}

bool MainRenderer::EnsureRuntime2DOverlayTarget(UINT width, UINT height){
	if(!m_graphicsContext || !m_graphicsContext->GetDevice() ||
		width == 0 || height == 0){
		return false;
	}

	if(m_runtime2DOverlayTexture && m_runtime2DOverlayRtv &&
		m_runtime2DOverlaySrv && m_runtime2DOverlayWidth == width &&
		m_runtime2DOverlayHeight == height){
		return true;
	}

	ReleaseRuntime2DOverlayTarget();

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	ID3D11Device* device = m_graphicsContext->GetDevice();
	HRESULT hr = device->CreateTexture2D(
		&desc,
		nullptr,
		m_runtime2DOverlayTexture.ReleaseAndGetAddressOf()
	);
	if(FAILED(hr) || !m_runtime2DOverlayTexture){
		ReleaseRuntime2DOverlayTarget();
		return false;
	}

	hr = device->CreateRenderTargetView(
		m_runtime2DOverlayTexture.Get(),
		nullptr,
		m_runtime2DOverlayRtv.ReleaseAndGetAddressOf()
	);
	if(FAILED(hr) || !m_runtime2DOverlayRtv){
		ReleaseRuntime2DOverlayTarget();
		return false;
	}

	hr = device->CreateShaderResourceView(
		m_runtime2DOverlayTexture.Get(),
		nullptr,
		m_runtime2DOverlaySrv.ReleaseAndGetAddressOf()
	);
	if(FAILED(hr) || !m_runtime2DOverlaySrv){
		ReleaseRuntime2DOverlayTarget();
		return false;
	}

	m_runtime2DOverlayWidth = width;
	m_runtime2DOverlayHeight = height;
	return true;
}

void MainRenderer::ReleaseRuntime2DOverlayTarget() noexcept {
	m_runtime2DOverlaySrv.Reset();
	m_runtime2DOverlayRtv.Reset();
	m_runtime2DOverlayTexture.Reset();
	m_runtime2DOverlayWidth = 0;
	m_runtime2DOverlayHeight = 0;
}

ID3D11ShaderResourceView* MainRenderer::RenderRuntime2DOverlay(
	UINT width,
	UINT height
){
	if(m_runtime2DCommands.empty()){
		return nullptr;
	}

	if(!m_d2dRenderer || !EnsureRuntime2DOverlayTarget(width, height)){
		m_runtime2DCommands.clear();
		return nullptr;
	}

	if(!m_d2dRenderer->BeginTextureDraw(m_runtime2DOverlayTexture.Get())){
		m_runtime2DCommands.clear();
		return nullptr;
	}

	for(const Runtime2DCommand& command : m_runtime2DCommands){
		switch(command.type){
		case Runtime2DCommandType::Text:
			m_d2dRenderer->DrawTextToTexture(
				command.text,
				command.x,
				command.y,
				command.fontSize,
				command.color
			);
			break;

		case Runtime2DCommandType::FillRect:
			m_d2dRenderer->FillRectToTexture(
				command.x,
				command.y,
				command.width,
				command.height,
				command.color
			);
			break;
		}
	}

	const bool drawSucceeded = m_d2dRenderer->EndTextureDraw();
	m_runtime2DCommands.clear();
	return drawSucceeded ? m_runtime2DOverlaySrv.Get() : nullptr;
}

void MainRenderer::BeginFrame() {
	// 前Frameが途中終了してもRuntime UIを持ち越さない。
	m_runtime2DCommands.clear();

	if(!m_graphicsContext || m_graphicsContext->IsDeviceLost()){
		return;
	}

	float clearColor[4] = {0.5f, 0.5f, 0.5f, 1.0f};
	m_graphicsContext->Clear(clearColor);
}

void MainRenderer::EndFrame(bool vsync) {
	if(!m_graphicsContext || m_graphicsContext->IsDeviceLost()){
		m_runtime2DCommands.clear();
		m_gpuPassTimingProfiler.Reset();
		return;
	}

	m_graphicsContext->Present(vsync);

	// PresentでDevice Lostが確定した時点で、現在Frameを含む未回収Timestamp Queryを
	// GetData待機なしで破棄する。次Frameで失われたDeviceのQueryを参照しない。
	if(m_graphicsContext->IsDeviceLost()){
		m_gpuPassTimingProfiler.Reset();
	}
}
