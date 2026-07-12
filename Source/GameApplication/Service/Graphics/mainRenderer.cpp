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

void MainRenderer::FlushRuntime2DOverlay(){
	if(!m_d2dRenderer){
		m_runtime2DCommands.clear();
		return;
	}

	for(const Runtime2DCommand& command : m_runtime2DCommands){
		switch(command.type){
		case Runtime2DCommandType::Text:
			m_d2dRenderer->DrawText2D(
				command.text,
				command.x,
				command.y,
				command.fontSize,
				command.color
			);
			break;

		case Runtime2DCommandType::FillRect:
			m_d2dRenderer->FillRect2D(
				command.x,
				command.y,
				command.width,
				command.height,
				command.color
			);
			break;
		}
	}

	m_runtime2DCommands.clear();
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
