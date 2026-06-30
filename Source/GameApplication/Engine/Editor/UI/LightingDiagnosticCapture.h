#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Graphics/GpuFrameTiming.h"

class LightingDiagnosticCapture final {
public:
	struct MetricSummary {
		double averageMilliseconds = 0.0;
		double p95Milliseconds = 0.0;
	};

	struct Summary {
		std::string label;
		std::size_t sampleCount = 0;
		MetricSummary gpuFrame;
		MetricSummary playerLighting;
		MetricSummary playerShadow;
		MetricSummary playerPostEffect;
		bool valid = false;
	};

	void Start(
		std::string label,
		std::size_t warmupFrameCount = 60,
		std::size_t targetSampleCount = 120
	){
		m_label = std::move(label);
		m_warmupRemaining = warmupFrameCount;
		m_targetSampleCount = (std::max)(targetSampleCount, std::size_t{1});
		m_lastConsumedFrameSerial = 0;
		m_gpuFrameSamples.clear();
		m_playerLightingSamples.clear();
		m_playerShadowSamples.clear();
		m_playerPostEffectSamples.clear();
		m_gpuFrameSamples.reserve(m_targetSampleCount);
		m_playerLightingSamples.reserve(m_targetSampleCount);
		m_playerShadowSamples.reserve(m_targetSampleCount);
		m_playerPostEffectSamples.reserve(m_targetSampleCount);
		m_summary = Summary{};
		m_capturing = true;
	}

	void Cancel() noexcept {
		m_capturing = false;
	}

	void Consume(std::span<const GpuFrameTimingResult> results){
		if(!m_capturing){
			return;
		}

		for(const GpuFrameTimingResult& result : results){
			if(!m_capturing){
				break;
			}
			if(result.frameSerial == 0 ||
				result.frameSerial <= m_lastConsumedFrameSerial ||
				result.status != GpuFrameTimingStatus::Resolved ||
				!result.IsPassResolved(GpuPassTimingScope::PlayerLighting)){
				continue;
			}

			m_lastConsumedFrameSerial = result.frameSerial;
			if(m_warmupRemaining > 0){
				--m_warmupRemaining;
				continue;
			}

			m_gpuFrameSamples.push_back(result.seconds * 1000.0);
			m_playerLightingSamples.push_back(
				result.GetPassSeconds(GpuPassTimingScope::PlayerLighting) * 1000.0
			);
			m_playerShadowSamples.push_back(
				result.IsPassResolved(GpuPassTimingScope::PlayerShadow)
				? result.GetPassSeconds(GpuPassTimingScope::PlayerShadow) * 1000.0
				: 0.0
			);
			m_playerPostEffectSamples.push_back(
				result.IsPassResolved(GpuPassTimingScope::PlayerPostEffect)
				? result.GetPassSeconds(GpuPassTimingScope::PlayerPostEffect) * 1000.0
				: 0.0
			);

			if(m_gpuFrameSamples.size() >= m_targetSampleCount){
				Finalize();
			}
		}
	}

	bool IsCapturing() const noexcept {
		return m_capturing;
	}

	std::size_t WarmupRemaining() const noexcept {
		return m_warmupRemaining;
	}

	std::size_t SamplesCollected() const noexcept {
		return m_gpuFrameSamples.size();
	}

	std::size_t TargetSampleCount() const noexcept {
		return m_targetSampleCount;
	}

	const Summary& GetSummary() const noexcept {
		return m_summary;
	}

private:
	static MetricSummary CalculateMetric(
		const std::vector<double>& samples
	){
		MetricSummary result;
		if(samples.empty()){
			return result;
		}

		double total = 0.0;
		for(double sample : samples){
			total += sample;
		}
		result.averageMilliseconds = total / static_cast<double>(samples.size());

		std::vector<double> ordered = samples;
		std::sort(ordered.begin(), ordered.end());
		const std::size_t p95Rank = static_cast<std::size_t>(
			std::ceil(static_cast<double>(ordered.size()) * 0.95)
		);
		const std::size_t p95Index = (std::min)(
			(std::max)(p95Rank, std::size_t{1}) - 1,
			ordered.size() - 1
		);
		result.p95Milliseconds = ordered[p95Index];
		return result;
	}

	void Finalize(){
		m_summary.label = m_label;
		m_summary.sampleCount = m_gpuFrameSamples.size();
		m_summary.gpuFrame = CalculateMetric(m_gpuFrameSamples);
		m_summary.playerLighting = CalculateMetric(m_playerLightingSamples);
		m_summary.playerShadow = CalculateMetric(m_playerShadowSamples);
		m_summary.playerPostEffect = CalculateMetric(m_playerPostEffectSamples);
		m_summary.valid = true;
		m_capturing = false;
	}

	std::string m_label;
	std::size_t m_warmupRemaining = 0;
	std::size_t m_targetSampleCount = 120;
	std::uint64_t m_lastConsumedFrameSerial = 0;
	bool m_capturing = false;

	std::vector<double> m_gpuFrameSamples;
	std::vector<double> m_playerLightingSamples;
	std::vector<double> m_playerShadowSamples;
	std::vector<double> m_playerPostEffectSamples;
	Summary m_summary;
};
