#pragma once

#include "Resources/Data/audioData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace MiniGameCollection::Presentation {

struct ProceduralToneDescription {
    float frequencyHz = 440.0f;
    float durationSeconds = 0.12f;
    float volume = 0.45f;
    float attackSeconds = 0.006f;
    float releaseSeconds = 0.055f;
    float secondHarmonic = 0.18f;
    float frequencySlideHz = 0.0f;
};

class MiniGameProceduralAudio {
public:
    static std::shared_ptr<AudioData> CreateTone(
        std::string debugName,
        ProceduralToneDescription description,
        std::uint32_t sampleRate = 44100
    ) {
        description.frequencyHz = std::max(20.0f, description.frequencyHz);
        description.durationSeconds = std::clamp(
            description.durationSeconds,
            0.02f,
            2.0f
        );
        description.volume = std::clamp(description.volume, 0.0f, 0.95f);
        description.attackSeconds = std::max(0.0f, description.attackSeconds);
        description.releaseSeconds = std::max(0.0f, description.releaseSeconds);

        constexpr std::uint16_t channelCount = 1;
        constexpr std::uint16_t bitsPerSample = 16;
        constexpr float pi = 3.14159265358979323846f;
        const std::uint32_t sampleCount = std::max<std::uint32_t>(
            1,
            static_cast<std::uint32_t>(
                description.durationSeconds * static_cast<float>(sampleRate)
            )
        );
        const std::uint32_t byteCount =
            sampleCount * channelCount * (bitsPerSample / 8u);

        auto data = std::make_shared<AudioData>();
        data->FilePath = std::move(debugName);
        data->m_Length = static_cast<int>(byteCount);
        data->m_PlayLength = static_cast<int>(sampleCount);
        data->m_SoundData = new BYTE[byteCount]{};

        data->m_Format.wFormatTag = WAVE_FORMAT_PCM;
        data->m_Format.nChannels = channelCount;
        data->m_Format.nSamplesPerSec = sampleRate;
        data->m_Format.wBitsPerSample = bitsPerSample;
        data->m_Format.nBlockAlign =
            channelCount * (bitsPerSample / 8u);
        data->m_Format.nAvgBytesPerSec =
            sampleRate * data->m_Format.nBlockAlign;
        data->m_Format.cbSize = 0;

        auto* samples = reinterpret_cast<std::int16_t*>(data->m_SoundData);
        float phase = 0.0f;
        for (std::uint32_t index = 0; index < sampleCount; ++index) {
            const float time = static_cast<float>(index) /
                static_cast<float>(sampleRate);
            const float normalized = sampleCount > 1
                ? static_cast<float>(index) / static_cast<float>(sampleCount - 1)
                : 0.0f;
            const float frequency = std::max(
                20.0f,
                description.frequencyHz +
                    description.frequencySlideHz * normalized
            );
            phase += 2.0f * pi * frequency / static_cast<float>(sampleRate);

            const float attack = description.attackSeconds > 0.0f
                ? std::clamp(time / description.attackSeconds, 0.0f, 1.0f)
                : 1.0f;
            const float remaining = description.durationSeconds - time;
            const float release = description.releaseSeconds > 0.0f
                ? std::clamp(remaining / description.releaseSeconds, 0.0f, 1.0f)
                : 1.0f;
            const float envelope = attack * release;

            const float fundamental = std::sin(phase);
            const float harmonic = std::sin(phase * 2.0f) *
                description.secondHarmonic;
            const float value = std::clamp(
                (fundamental + harmonic) * description.volume * envelope,
                -1.0f,
                1.0f
            );
            samples[index] = static_cast<std::int16_t>(value * 32767.0f);
        }

        return data;
    }

    static ProceduralToneDescription Countdown() noexcept {
        return {
            .frequencyHz = 520.0f,
            .durationSeconds = 0.1f,
            .volume = 0.42f,
            .attackSeconds = 0.003f,
            .releaseSeconds = 0.05f,
            .secondHarmonic = 0.1f,
            .frequencySlideHz = 40.0f
        };
    }

    static ProceduralToneDescription Go() noexcept {
        return {
            .frequencyHz = 740.0f,
            .durationSeconds = 0.18f,
            .volume = 0.5f,
            .attackSeconds = 0.004f,
            .releaseSeconds = 0.08f,
            .secondHarmonic = 0.22f,
            .frequencySlideHz = 180.0f
        };
    }

    static ProceduralToneDescription Score() noexcept {
        return {
            .frequencyHz = 660.0f,
            .durationSeconds = 0.11f,
            .volume = 0.45f,
            .attackSeconds = 0.002f,
            .releaseSeconds = 0.06f,
            .secondHarmonic = 0.16f,
            .frequencySlideHz = 120.0f
        };
    }

    static ProceduralToneDescription Hit() noexcept {
        return {
            .frequencyHz = 145.0f,
            .durationSeconds = 0.15f,
            .volume = 0.55f,
            .attackSeconds = 0.001f,
            .releaseSeconds = 0.1f,
            .secondHarmonic = 0.42f,
            .frequencySlideHz = -85.0f
        };
    }

    static ProceduralToneDescription Failure() noexcept {
        return {
            .frequencyHz = 300.0f,
            .durationSeconds = 0.28f,
            .volume = 0.46f,
            .attackSeconds = 0.005f,
            .releaseSeconds = 0.14f,
            .secondHarmonic = 0.12f,
            .frequencySlideHz = -180.0f
        };
    }

    static ProceduralToneDescription Result() noexcept {
        return {
            .frequencyHz = 440.0f,
            .durationSeconds = 0.32f,
            .volume = 0.45f,
            .attackSeconds = 0.006f,
            .releaseSeconds = 0.12f,
            .secondHarmonic = 0.24f,
            .frequencySlideHz = 360.0f
        };
    }
};

} // namespace MiniGameCollection::Presentation
