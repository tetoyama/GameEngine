#include "Game/MiniGameCollection/Presentation/PresentationSpikeModel.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

namespace {

class FakePresentationBackend final
    : public MiniGameCollection::Presentation::IMiniGamePresentationBackend {
public:
    void PlayOneShotEffect(
        const MiniGameCollection::Presentation::EffectCueRequest& request
    ) override {
        AssertToken(request.sceneToken);
        effects.emplace_back(request.cueId);
    }

    void PlayOneShotSound(
        const MiniGameCollection::Presentation::SoundCueRequest& request
    ) override {
        AssertToken(request.sceneToken);
        sounds.emplace_back(request.cueId);
        pitches.push_back(request.pitch);
    }

    void PlayCameraShake(
        const MiniGameCollection::Presentation::CameraShakeRequest& request
    ) override {
        AssertToken(request.sceneToken);
        ++cameraShakeCount;
    }

    void PlayScreenFlash(
        const MiniGameCollection::Presentation::ScreenFlashRequest& request
    ) override {
        AssertToken(request.sceneToken);
        ++screenFlashCount;
    }

    void PlayUiTween(
        const MiniGameCollection::Presentation::UiTweenRequest& request
    ) override {
        AssertToken(request.sceneToken);
        uiTargets.emplace_back(request.targetId);
    }

    void CancelAllForScene(MiniGameCollection::SceneToken sceneToken) override {
        AssertToken(sceneToken);
        ++cancelCount;
        activeTransientCount = 0;
    }

    void AssertToken(MiniGameCollection::SceneToken sceneToken) {
        assert(sceneToken == expectedToken);
        ++activeTransientCount;
    }

    MiniGameCollection::SceneToken expectedToken = 9001;
    std::vector<std::string> effects;
    std::vector<std::string> sounds;
    std::vector<std::string> uiTargets;
    std::vector<float> pitches;
    std::size_t cameraShakeCount = 0;
    std::size_t screenFlashCount = 0;
    std::size_t cancelCount = 0;
    std::size_t activeTransientCount = 0;
};

void RunSuccessfulAttempt(
    MiniGameCollection::Presentation::PresentationSpikeModel& spike,
    MiniGameCollection::Presentation::MiniGamePresentationService& presentation
) {
    using namespace MiniGameCollection::Presentation;

    spike.Tick(3.0f);
    assert(spike.GetPhase() == PresentationSpikePhase::InputWindow);

    spike.Tick(0.65f);
    assert(spike.SubmitInput());
    assert(spike.GetOutcome() == PresentationSpikeOutcome::Success);
    assert(spike.GetPhase() == PresentationSpikePhase::Outcome);

    spike.Tick(0.6f);
    assert(spike.GetPhase() == PresentationSpikePhase::Result);

    spike.Tick(1.2f);
    assert(spike.GetPhase() == PresentationSpikePhase::RetryReady);
    assert(presentation.PendingCueCount() == 0);
}

} // namespace

int main() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::Presentation;

    FakePresentationBackend backend;
    MiniGamePresentationService presentation(backend);
    PresentationSpikeModel spike(presentation);

    spike.Begin(backend.expectedToken);
    for (int attempt = 0; attempt < 10; ++attempt) {
        RunSuccessfulAttempt(spike, presentation);
        if (attempt < 9) {
            const std::size_t cancelBeforeRetry = backend.cancelCount;
            assert(spike.Retry());
            assert(backend.cancelCount == cancelBeforeRetry + 1);
            assert(spike.GetPhase() == PresentationSpikePhase::Countdown);
            assert(presentation.PendingCueCount() == 4);
        }
    }

    assert(spike.GetRetryCount() == 9);
    assert(backend.effects.size() == 10);
    assert(backend.screenFlashCount == 10);
    assert(backend.cameraShakeCount >= 20);

    const std::size_t cancelBeforeShutdown = backend.cancelCount;
    spike.Shutdown();
    assert(backend.cancelCount == cancelBeforeShutdown + 1);
    assert(presentation.PendingCueCount() == 0);
    assert(backend.activeTransientCount == 0);
    return 0;
}
