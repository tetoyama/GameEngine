#pragma once

#include <d3d11.h>
#include <wrl/client.h>

// Static BatchのRHI PipelineはD3D11 Immediate ContextへVertex Shaderと
// Input Layoutを直接Bindする。通常RenderPacketへ戻る前に元の状態を復元し、
// INSTANCEWORLD / INSTANCEOBJECTを要求するLayoutが通常VSへ残留することを防ぐ。
class StaticBatchD3D11VertexInputState final {
public:
	explicit StaticBatchD3D11VertexInputState(
		ID3D11DeviceContext* context
	) noexcept
		: m_context(context) {
		if(!m_context) return;

		m_context->VSGetShader(
			m_vertexShader.ReleaseAndGetAddressOf(),
			nullptr,
			nullptr
		);
		m_context->IAGetInputLayout(
			m_inputLayout.ReleaseAndGetAddressOf()
		);
		m_context->IAGetPrimitiveTopology(&m_topology);
		m_captured = true;
	}

	~StaticBatchD3D11VertexInputState(){
		Restore();
	}

	StaticBatchD3D11VertexInputState(
		const StaticBatchD3D11VertexInputState&
	) = delete;
	StaticBatchD3D11VertexInputState& operator=(
		const StaticBatchD3D11VertexInputState&
	) = delete;
	StaticBatchD3D11VertexInputState(
		StaticBatchD3D11VertexInputState&&
	) = delete;
	StaticBatchD3D11VertexInputState& operator=(
		StaticBatchD3D11VertexInputState&&
	) = delete;

	void Restore() noexcept {
		if(!m_captured || !m_context) return;

		m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		m_context->IASetInputLayout(m_inputLayout.Get());
		m_context->IASetPrimitiveTopology(m_topology);
		m_captured = false;
	}

	bool IsCaptured() const noexcept {
		return m_captured;
	}

private:
	ID3D11DeviceContext* m_context = nullptr;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
	D3D11_PRIMITIVE_TOPOLOGY m_topology =
		D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	bool m_captured = false;
};
