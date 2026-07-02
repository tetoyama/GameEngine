#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "GpuFrameTiming.h"

// D3D11 Timestamp Queryを使用する非同期GPU Pass Profiler。
// MainRendererが所有し、同一Frame内のPlayer / Editor描画区間を一つの
// Frame Serialへ集約する。Query結果待機によるCPU Stallは禁止する。
class GpuPassTimingProfiler final {
public:
	void BeginFrame(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		std::uint64_t frameSerial
	){
		Resolve(context);
		if(!device || !context || !EnsureQueries(device)){
			PushStatus(frameSerial, GpuFrameTimingStatus::Dropped);
			m_activeQueryIndex = -1;
			return;
		}

		if(m_activeQueryIndex >= 0){
			EndFrame(context);
		}

		QuerySet& querySet = m_querySets[m_writeIndex];
		if(querySet.pending){
			// GPUがリング長以上遅れている場合もCPUを待機させない。
			PushStatus(frameSerial, GpuFrameTimingStatus::Dropped);
			m_activeQueryIndex = -1;
			return;
		}

		querySet.frameSerial = frameSerial;
		for(PassQuery& pass : querySet.passes){
			pass.used = false;
			pass.active = false;
		}

		m_activeQueryIndex = static_cast<int>(m_writeIndex);
		context->Begin(querySet.disjoint.Get());
		context->End(querySet.frameBegin.Get());
	}

	bool BeginPass(
		ID3D11DeviceContext* context,
		GpuPassTimingScope scope
	) noexcept {
		if(!context || m_activeQueryIndex < 0){
			return false;
		}

		const std::size_t scopeIndex = static_cast<std::size_t>(scope);
		if(scopeIndex >= GpuPassTimingScopeCount){
			return false;
		}

		PassQuery& pass =
			m_querySets[static_cast<std::size_t>(m_activeQueryIndex)]
				.passes[scopeIndex];
		if(pass.used || pass.active || !pass.begin || !pass.end){
			return false;
		}

		context->End(pass.begin.Get());
		pass.used = true;
		pass.active = true;
		return true;
	}

	void EndPass(
		ID3D11DeviceContext* context,
		GpuPassTimingScope scope
	) noexcept {
		if(!context || m_activeQueryIndex < 0){
			return;
		}

		const std::size_t scopeIndex = static_cast<std::size_t>(scope);
		if(scopeIndex >= GpuPassTimingScopeCount){
			return;
		}

		PassQuery& pass =
			m_querySets[static_cast<std::size_t>(m_activeQueryIndex)]
				.passes[scopeIndex];
		if(!pass.active || !pass.end){
			return;
		}
		context->End(pass.end.Get());
		pass.active = false;
	}

	void EndFrame(ID3D11DeviceContext* context) noexcept {
		if(!context || m_activeQueryIndex < 0){
			return;
		}

		QuerySet& querySet =
			m_querySets[static_cast<std::size_t>(m_activeQueryIndex)];
		for(PassQuery& pass : querySet.passes){
			if(!pass.active){
				continue;
			}
			context->End(pass.end.Get());
			pass.active = false;
		}

		context->End(querySet.frameEnd.Get());
		context->End(querySet.disjoint.Get());
		querySet.pending = true;

		m_writeIndex = (m_writeIndex + 1) % kBufferedFrames;
		m_activeQueryIndex = -1;
	}

	std::vector<GpuFrameTimingResult> ConsumeResolved(
		ID3D11DeviceContext* context
	){
		Resolve(context);
		std::stable_sort(
			m_resolved.begin(),
			m_resolved.end(),
			[](const GpuFrameTimingResult& lhs, const GpuFrameTimingResult& rhs){
				return lhs.frameSerial < rhs.frameSerial;
			}
		);

		std::vector<GpuFrameTimingResult> results;
		results.swap(m_resolved);
		return results;
	}

	void Reset() noexcept {
		for(QuerySet& querySet : m_querySets){
			querySet.disjoint.Reset();
			querySet.frameBegin.Reset();
			querySet.frameEnd.Reset();
			querySet.frameSerial = 0;
			querySet.pending = false;
			for(PassQuery& pass : querySet.passes){
				pass.begin.Reset();
				pass.end.Reset();
				pass.used = false;
				pass.active = false;
			}
		}
		m_device = nullptr;
		m_writeIndex = 0;
		m_activeQueryIndex = -1;
		m_available = false;
		m_resolved.clear();
	}

private:
	struct PassQuery {
		Microsoft::WRL::ComPtr<ID3D11Query> begin;
		Microsoft::WRL::ComPtr<ID3D11Query> end;
		bool used = false;
		bool active = false;
	};

	struct QuerySet {
		Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
		Microsoft::WRL::ComPtr<ID3D11Query> frameBegin;
		Microsoft::WRL::ComPtr<ID3D11Query> frameEnd;
		std::array<PassQuery, GpuPassTimingScopeCount> passes{};
		std::uint64_t frameSerial = 0;
		bool pending = false;
	};

	enum class QueryReadState : std::uint8_t {
		Ready,
		Pending,
		Failed
	};

	static constexpr std::size_t kBufferedFrames = 4;

	static QueryReadState ReadTimestamp(
		ID3D11DeviceContext* context,
		ID3D11Query* query,
		UINT64& value
	) noexcept {
		if(!context || !query){
			return QueryReadState::Failed;
		}
		const HRESULT result = context->GetData(
			query,
			&value,
			sizeof(value),
			D3D11_ASYNC_GETDATA_DONOTFLUSH
		);
		if(result == S_FALSE){
			return QueryReadState::Pending;
		}
		return SUCCEEDED(result)
			? QueryReadState::Ready
			: QueryReadState::Failed;
	}

	bool EnsureQueries(ID3D11Device* device){
		if(m_available && m_device == device){
			return true;
		}
		Reset();
		if(!device){
			return false;
		}

		D3D11_QUERY_DESC desc{};
		for(QuerySet& querySet : m_querySets){
			desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
			if(FAILED(device->CreateQuery(
				&desc,
				querySet.disjoint.ReleaseAndGetAddressOf()
			))){
				Reset();
				return false;
			}

			desc.Query = D3D11_QUERY_TIMESTAMP;
			if(FAILED(device->CreateQuery(
				&desc,
				querySet.frameBegin.ReleaseAndGetAddressOf()
			)) || FAILED(device->CreateQuery(
				&desc,
				querySet.frameEnd.ReleaseAndGetAddressOf()
			))){
				Reset();
				return false;
			}

			for(PassQuery& pass : querySet.passes){
				if(FAILED(device->CreateQuery(
					&desc,
					pass.begin.ReleaseAndGetAddressOf()
				)) || FAILED(device->CreateQuery(
					&desc,
					pass.end.ReleaseAndGetAddressOf()
				))){
					Reset();
					return false;
				}
			}
		}

		m_device = device;
		m_available = true;
		return true;
	}

	void PushStatus(
		std::uint64_t frameSerial,
		GpuFrameTimingStatus status
	){
		GpuFrameTimingResult result;
		result.frameSerial = frameSerial;
		result.status = status;
		m_resolved.push_back(result);
	}

	void Complete(
		QuerySet& querySet,
		GpuFrameTimingResult result
	){
		m_resolved.push_back(std::move(result));
		querySet.frameSerial = 0;
		querySet.pending = false;
		for(PassQuery& pass : querySet.passes){
			pass.used = false;
			pass.active = false;
		}
	}

	void Resolve(ID3D11DeviceContext* context){
		if(!m_available || !context){
			return;
		}

		for(QuerySet& querySet : m_querySets){
			if(!querySet.pending){
				continue;
			}

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
			const HRESULT disjointResult = context->GetData(
				querySet.disjoint.Get(),
				&disjoint,
				sizeof(disjoint),
				D3D11_ASYNC_GETDATA_DONOTFLUSH
			);
			if(disjointResult == S_FALSE){
				continue;
			}

			GpuFrameTimingResult result;
			result.frameSerial = querySet.frameSerial;
			if(FAILED(disjointResult) || disjoint.Disjoint ||
				disjoint.Frequency == 0){
				result.status = GpuFrameTimingStatus::Invalid;
				Complete(querySet, std::move(result));
				continue;
			}

			UINT64 frameBegin = 0;
			UINT64 frameEnd = 0;
			const QueryReadState frameBeginState = ReadTimestamp(
				context,
				querySet.frameBegin.Get(),
				frameBegin
			);
			const QueryReadState frameEndState = ReadTimestamp(
				context,
				querySet.frameEnd.Get(),
				frameEnd
			);
			if(frameBeginState == QueryReadState::Pending ||
				frameEndState == QueryReadState::Pending){
				continue;
			}
			if(frameBeginState == QueryReadState::Failed ||
				frameEndState == QueryReadState::Failed ||
				frameEnd < frameBegin){
				result.status = GpuFrameTimingStatus::Invalid;
				Complete(querySet, std::move(result));
				continue;
			}

			bool passPending = false;
			for(std::size_t index = 0; index < querySet.passes.size(); ++index){
				PassQuery& pass = querySet.passes[index];
				if(!pass.used){
					continue;
				}

				UINT64 passBegin = 0;
				UINT64 passEnd = 0;
				const QueryReadState passBeginState = ReadTimestamp(
					context,
					pass.begin.Get(),
					passBegin
				);
				const QueryReadState passEndState = ReadTimestamp(
					context,
					pass.end.Get(),
					passEnd
				);
				if(passBeginState == QueryReadState::Pending ||
					passEndState == QueryReadState::Pending){
					passPending = true;
					break;
				}

				const std::uint64_t bit = 1ull << index;
				if(passBeginState == QueryReadState::Failed ||
					passEndState == QueryReadState::Failed ||
					passEnd < passBegin){
					result.invalidPassMask |= bit;
					continue;
				}

				result.passSeconds[index] =
					static_cast<double>(passEnd - passBegin) /
					static_cast<double>(disjoint.Frequency);
				result.resolvedPassMask |= bit;
			}
			if(passPending){
				continue;
			}

			result.seconds =
				static_cast<double>(frameEnd - frameBegin) /
				static_cast<double>(disjoint.Frequency);
			result.status = GpuFrameTimingStatus::Resolved;
			Complete(querySet, std::move(result));
		}
	}

	ID3D11Device* m_device = nullptr;
	std::array<QuerySet, kBufferedFrames> m_querySets{};
	std::size_t m_writeIndex = 0;
	int m_activeQueryIndex = -1;
	bool m_available = false;
	std::vector<GpuFrameTimingResult> m_resolved;
};

class ScopedGpuPassTiming final {
public:
	ScopedGpuPassTiming(
		GpuPassTimingProfiler& profiler,
		ID3D11DeviceContext* context,
		GpuPassTimingScope scope
	) noexcept
		: m_profiler(&profiler),
		  m_context(context),
		  m_scope(scope),
		  m_active(profiler.BeginPass(context, scope)) {
	}

	~ScopedGpuPassTiming(){
		if(m_active && m_profiler){
			m_profiler->EndPass(m_context, m_scope);
		}
	}

	ScopedGpuPassTiming(const ScopedGpuPassTiming&) = delete;
	ScopedGpuPassTiming& operator=(const ScopedGpuPassTiming&) = delete;

private:
	GpuPassTimingProfiler* m_profiler = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	GpuPassTimingScope m_scope = GpuPassTimingScope::PlayerGBuffer;
	bool m_active = false;
};
