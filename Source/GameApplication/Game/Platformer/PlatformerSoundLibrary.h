#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

class PlatformerSoundLibrary {
public:
	static constexpr const char* CoinPath = "Asset/Game/Platformer/Sound/Coin.wav";
	static constexpr const char* ActionPath = "Asset/Game/Platformer/Sound/Action.wav";
	static constexpr const char* ImpactPath = "Asset/Game/Platformer/Sound/Impact.wav";
	static constexpr const char* CheckpointPath = "Asset/Game/Platformer/Sound/Checkpoint.wav";
	static constexpr const char* BossChargePath = "Asset/Game/Platformer/Sound/BossCharge.wav";
	static constexpr const char* ClearPath = "Asset/Game/Platformer/Sound/Clear.wav";

	static void EnsureGenerated() {
		std::error_code error;
		std::filesystem::create_directories("Asset/Game/Platformer/Sound", error);
		WriteIfMissing(CoinPath, MakeCoin());
		WriteIfMissing(ActionPath, MakeChirp(280.0f, 1050.0f, 0.15f, 0.45f, 0.08f));
		WriteIfMissing(ImpactPath, MakeImpact());
		WriteIfMissing(CheckpointPath, MakeArpeggio({
			{0.00f, 523.25f}, {0.07f, 659.25f}, {0.14f, 783.99f}, {0.21f, 1046.50f}
		}, 0.32f, 0.26f));
		WriteIfMissing(BossChargePath, MakeChirp(90.0f, 520.0f, 0.34f, 0.50f, 0.10f));
		WriteIfMissing(ClearPath, MakeArpeggio({
			{0.00f, 523.25f}, {0.11f, 659.25f}, {0.22f, 783.99f},
			{0.35f, 1046.50f}, {0.50f, 1318.50f}
		}, 0.72f, 0.23f));
	}

private:
	static constexpr uint32_t SampleRate = 8000;
	static constexpr float Pi = 3.14159265358979323846f;

	static bool IsUsableFile(const char* path) {
		std::error_code error;
		return std::filesystem::exists(path, error) &&
			std::filesystem::is_regular_file(path, error) &&
			std::filesystem::file_size(path, error) > 44;
	}

	static void WriteIfMissing(const char* path, const std::vector<int16_t>& samples) {
		if(IsUsableFile(path) || samples.empty()) return;
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if(!output) return;

		const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
		WriteTag(output, "RIFF");
		WriteU32(output, 36u + dataBytes);
		WriteTag(output, "WAVE");
		WriteTag(output, "fmt ");
		WriteU32(output, 16u);
		WriteU16(output, 1u);
		WriteU16(output, 1u);
		WriteU32(output, SampleRate);
		WriteU32(output, SampleRate * sizeof(int16_t));
		WriteU16(output, sizeof(int16_t));
		WriteU16(output, 16u);
		WriteTag(output, "data");
		WriteU32(output, dataBytes);
		output.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(dataBytes));
	}

	static std::vector<int16_t> MakeCoin() {
		const size_t count = static_cast<size_t>(SampleRate * 0.13f);
		std::vector<float> mixed(count, 0.0f);
		AddTone(mixed, 0.00f, 900.0f, 0.38f, 0.13f, 9.0f);
		AddTone(mixed, 0.04f, 1350.0f, 0.38f, 0.09f, 9.0f);
		return Quantize(mixed);
	}

	static std::vector<int16_t> MakeChirp(
		float startFrequency,
		float endFrequency,
		float duration,
		float amplitude,
		float noiseAmount
	) {
		const size_t count = static_cast<size_t>(SampleRate * duration);
		std::vector<float> samples(count, 0.0f);
		uint32_t noiseState = 0x9e3779b9u;
		for(size_t i = 0; i < count; ++i) {
			const float time = static_cast<float>(i) / static_cast<float>(SampleRate);
			const float normalized = duration > 0.0f ? time / duration : 0.0f;
			const float frequency = startFrequency + (endFrequency - startFrequency) * normalized;
			const float phase = 2.0f * Pi * (startFrequency * time +
				0.5f * (endFrequency - startFrequency) * time * normalized);
			noiseState = noiseState * 1664525u + 1013904223u;
			const float noise = (static_cast<float>((noiseState >> 8) & 0xffffu) / 32767.5f) - 1.0f;
			const float wave = std::sin(phase) * (1.0f - noiseAmount) + noise * noiseAmount;
			const float envelope = std::exp(-6.0f * normalized);
			const float attack = std::clamp(time / 0.004f, 0.0f, 1.0f);
			samples[i] = wave * envelope * attack * amplitude;
			(void)frequency;
		}
		return Quantize(samples);
	}

	static std::vector<int16_t> MakeImpact() {
		constexpr float duration = 0.18f;
		const size_t count = static_cast<size_t>(SampleRate * duration);
		std::vector<float> samples(count, 0.0f);
		uint32_t noiseState = 0x12345678u;
		for(size_t i = 0; i < count; ++i) {
			const float time = static_cast<float>(i) / static_cast<float>(SampleRate);
			const float normalized = time / duration;
			noiseState = noiseState * 1103515245u + 12345u;
			const float noise = (static_cast<float>((noiseState >> 8) & 0xffffu) / 32767.5f) - 1.0f;
			const float thump = std::sin(2.0f * Pi * (105.0f * time - 55.0f * time * time));
			samples[i] = 0.56f * thump * std::exp(-12.0f * normalized) +
				0.18f * noise * std::exp(-24.0f * normalized);
		}
		return Quantize(samples);
	}

	static std::vector<int16_t> MakeArpeggio(
		std::initializer_list<std::pair<float, float>> notes,
		float duration,
		float amplitude
	) {
		const size_t count = static_cast<size_t>(SampleRate * duration);
		std::vector<float> mixed(count, 0.0f);
		for(const auto& [start, frequency] : notes) {
			AddTone(mixed, start, frequency, amplitude, duration - start, 4.0f);
		}
		return Quantize(mixed);
	}

	static void AddTone(
		std::vector<float>& output,
		float startSeconds,
		float frequency,
		float amplitude,
		float duration,
		float decay
	) {
		const size_t start = static_cast<size_t>((std::max)(0.0f, startSeconds) * SampleRate);
		if(start >= output.size() || duration <= 0.0f) return;
		for(size_t i = start; i < output.size(); ++i) {
			const float localTime = static_cast<float>(i - start) / static_cast<float>(SampleRate);
			if(localTime > duration) break;
			const float normalized = localTime / duration;
			const float fundamental = std::sin(2.0f * Pi * frequency * localTime);
			const float harmonic = 0.20f * std::sin(4.0f * Pi * frequency * localTime);
			output[i] += (fundamental + harmonic) * amplitude * std::exp(-decay * normalized);
		}
	}

	static std::vector<int16_t> Quantize(const std::vector<float>& input) {
		std::vector<int16_t> output;
		output.reserve(input.size());
		for(float value : input) {
			const float clamped = std::clamp(value, -1.0f, 1.0f);
			output.push_back(static_cast<int16_t>(clamped * 32767.0f));
		}
		return output;
	}

	static void WriteTag(std::ofstream& output, const char tag[5]) {
		output.write(tag, 4);
	}

	static void WriteU16(std::ofstream& output, uint16_t value) {
		const char bytes[2] = {
			static_cast<char>(value & 0xffu),
			static_cast<char>((value >> 8u) & 0xffu)
		};
		output.write(bytes, 2);
	}

	static void WriteU32(std::ofstream& output, uint32_t value) {
		const char bytes[4] = {
			static_cast<char>(value & 0xffu),
			static_cast<char>((value >> 8u) & 0xffu),
			static_cast<char>((value >> 16u) & 0xffu),
			static_cast<char>((value >> 24u) & 0xffu)
		};
		output.write(bytes, 4);
	}
};
