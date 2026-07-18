#include "Game/MiniGameCollection/Runtime/MiniGameRuntimeMailbox.h"

#include <cassert>

int main() {
    using namespace MiniGameCollection;
    using namespace MiniGameCollection::Runtime;

    constexpr SceneToken practiceScene = 7001;

    MiniGameRuntimeMailbox::ClearForScene(practiceScene);
    MiniGameRuntimeMailbox::BeginPractice(practiceScene);
    assert(MiniGameRuntimeMailbox::IsPracticeActive(practiceScene));

    // 通常Result menuからの遷移はPractice中に受理しない。
    assert(!MiniGameRuntimeMailbox::SubmitTransition({
        .sceneToken = practiceScene,
        .sourceSceneName = "PracticeScene",
        .targetScenePath = "Match.scene",
        .reason = TransitionRequest::Retry
    }));

    // 練習の手触りに必要なScore/Hitは通常どおりPresentationへ流す。
    MiniGameRuntimeMailbox::SubmitPresentation({
        .type = RuntimePresentationCommandType::Score,
        .sceneToken = practiceScene,
        .intensity = 1.25f
    });
    MiniGameRuntimeMailbox::SubmitPresentation({
        .type = RuntimePresentationCommandType::Hit,
        .sceneToken = practiceScene,
        .intensity = 1.0f
    });
    auto presentation = MiniGameRuntimeMailbox::ConsumePresentation();
    assert(presentation.size() == 2);
    assert(presentation[0].type == RuntimePresentationCommandType::Score);
    assert(presentation[1].type == RuntimePresentationCommandType::Hit);

    // 公式Result/勝敗演出は流さず、Practice Overlayへ終了だけ通知する。
    MiniGameRuntimeMailbox::SubmitPresentation({
        .type = RuntimePresentationCommandType::Result,
        .sceneToken = practiceScene
    });
    MiniGameRuntimeMailbox::SubmitPresentation({
        .type = RuntimePresentationCommandType::Success,
        .sceneToken = practiceScene
    });
    assert(MiniGameRuntimeMailbox::ConsumePresentation().empty());
    assert(MiniGameRuntimeMailbox::ConsumePracticeRoundFinished(practiceScene));
    assert(!MiniGameRuntimeMailbox::ConsumePracticeRoundFinished(practiceScene));

    // OverlayだけがPracticeを維持したまま同一Sceneを再生成できる。
    assert(MiniGameRuntimeMailbox::SubmitPracticeTransition({
        .sceneToken = practiceScene,
        .sourceSceneName = "PracticeScene",
        .targetScenePath = "Practice.scene",
        .reason = TransitionRequest::Retry,
        .presentationWaitSeconds = 0.05f
    }));
    const auto reload = MiniGameRuntimeMailbox::ConsumeTransition();
    assert(reload.has_value());
    assert(reload->sceneToken == practiceScene);

    MiniGameRuntimeMailbox::EndPractice(practiceScene);
    assert(!MiniGameRuntimeMailbox::IsPracticeActive(practiceScene));
    MiniGameRuntimeMailbox::ClearForScene(practiceScene);
    return 0;
}
