from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


policy = '''#pragma once

namespace PlayerViewRefreshPolicy {

constexpr float IdleRefreshIntervalSeconds = 1.0f;

constexpr bool ShouldRender(
\tbool throttledState,
\tfloat elapsedSeconds,
\tbool hasOutput
) noexcept {
\treturn !throttledState ||
\t\t!hasOutput ||
\t\telapsedSeconds >= IdleRefreshIntervalSeconds;
}

} // namespace PlayerViewRefreshPolicy
'''
save(
    "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/RenderPass/PlayerView/PlayerViewRefreshPolicy.h",
    policy,
)


path = "Source/GameApplication/Engine/Scene/System/Render/RenderSystem/renderSystem.cpp"
text = load(path)
text = replace_once(
    text,
    '#include "RenderPass/PlayerView/PlayerPass.h"\n',
    '#include "RenderPass/PlayerView/PlayerPass.h"\n'
    '#include "RenderPass/PlayerView/PlayerViewRefreshPolicy.h"\n',
    "Player View refresh policy include",
)

text = replace_once(
    text,
    '''\t\tRenderPassContext renderPassContext(
\t\t\tRenderPhase::PHASE_GBUFFER,
\t\t\tplayerRenderLayerVisible,
\t\t\tFindCameraEntity(),
\t\t\tVector2(
\t\t\t\t(float)m_context->renderer->GetGraphicsContext()->m_width,
\t\t\t\t(float)m_context->renderer->GetGraphicsContext()->m_height
\t\t\t)
\t\t);
''',
    '''\t\tconst Vector2 fullScreenSize(
\t\t\tstatic_cast<float>(m_context->renderer->GetGraphicsContext()->m_width),
\t\t\tstatic_cast<float>(m_context->renderer->GetGraphicsContext()->m_height)
\t\t);
\t\tm_context->PlayerScreenSize = fullScreenSize;

\t\tRenderPassContext renderPassContext(
\t\t\tRenderPhase::PHASE_GBUFFER,
\t\t\tplayerRenderLayerVisible,
\t\t\tFindCameraEntity(),
\t\t\tfullScreenSize
\t\t);
''',
    "fullscreen Player View size synchronization",
)

old_player_view = '''void RenderSystem::PlayerView(){


\tImGui::Begin("Play View", showPlayer, 0);

\t// 共通UI（元のControlButtonやSeparatorなど）
\tControlButton();
\tImGui::Separator();

\t// カメラコンポーネントを持つエンティティ取得
\tconst CameraEntityData& cameraData = FindCameraEntity();
\tif(!cameraData.cameraComponent){
\t\tImGui::Text("No CameraBuffer Component found.");
\t\tImGui::End();
\t\treturn;
\t}
\t// 利用可能な領域サイズを取得
\tImVec2 avail = ImGui::GetContentRegionAvail();

\tif (avail.x <= 0.0f || avail.y <= 0.0f) {
\t\tImGui::End();
\t\treturn;
\t}
\tRenderPassContext renderPassContext(
\t\tRenderPhase::PHASE_GBUFFER,
\t\tplayerRenderLayerVisible,
\t\tcameraData,
\t\tVector2(avail.x, avail.y)
\t);

\tif(m_context->sceneManager->State == SceneManagerState::Stopped || m_context->sceneManager->State == SceneManagerState::Paused){
\t\tif(lazyTimer >= 1.0f){
\t\t\tlazyTimer = 0.0f;
\t\t\tm_PlayerPass->Execute(renderPassContext);
\t\t}
\t} else{
\t\tm_PlayerPass->Execute(renderPassContext);
\t}

\tImGui::Image((ImTextureRef)m_PlayerPass->result, avail);
\tImGui::End();

\tm_context->graphics->GetDeviceContext()->OMSetRenderTargets(
\t\t1, m_context->graphics->GetpRenderTargetView(), m_context->graphics->GetDepthStencilView()
\t);
}
'''

new_player_view = '''void RenderSystem::PlayerView(){

\tauto restoreMainRenderTarget = [this](){
\t\tm_context->graphics->GetDeviceContext()->OMSetRenderTargets(
\t\t\t1,
\t\t\tm_context->graphics->GetpRenderTargetView(),
\t\t\tm_context->graphics->GetDepthStencilView()
\t\t);
\t};

\tif(!ImGui::Begin("Play View", showPlayer, 0)){
\t\tImGui::End();
\t\trestoreMainRenderTarget();
\t\treturn;
\t}

\tControlButton();
\tImGui::Separator();

\tconst CameraEntityData cameraData = FindCameraEntity();
\tif(!cameraData.cameraComponent){
\t\tImGui::Text("No Camera Component found.");
\t\tImGui::End();
\t\trestoreMainRenderTarget();
\t\treturn;
\t}

\tconst ImVec2 avail = ImGui::GetContentRegionAvail();
\tif(avail.x <= 0.0f || avail.y <= 0.0f){
\t\tImGui::End();
\t\trestoreMainRenderTarget();
\t\treturn;
\t}

\tconst Vector2 playerSize(avail.x, avail.y);
\tm_context->PlayerScreenSize = playerSize;

\tRenderPassContext renderPassContext(
\t\tRenderPhase::PHASE_GBUFFER,
\t\tplayerRenderLayerVisible,
\t\tcameraData,
\t\tplayerSize
\t);

\tconst SceneManagerState state = m_context->sceneManager->State;
\tconst bool throttledState =
\t\tstate == SceneManagerState::Stopped ||
\t\tstate == SceneManagerState::Paused;
\tconst bool shouldRender = PlayerViewRefreshPolicy::ShouldRender(
\t\tthrottledState,
\t\tlazyTimer,
\t\tm_PlayerPass->result != nullptr
\t);

\tif(shouldRender){
\t\tif(throttledState) lazyTimer = 0.0f;
\t\tm_PlayerPass->Execute(renderPassContext);
\t}

\tif(m_PlayerPass->result){
\t\tImGui::Image((ImTextureRef)m_PlayerPass->result, avail);
\t}else{
\t\tImGui::TextDisabled("Player render output is not available.");
\t}
\tImGui::End();
\trestoreMainRenderTarget();
}
'''

text = replace_once(
    text,
    old_player_view,
    new_player_view,
    "Player View regression guard",
)
save(path, text)


test = '''#include <cassert>

#include "Engine/Scene/System/Render/RenderSystem/RenderPass/PlayerView/PlayerViewRefreshPolicy.h"

int main(){
\tusing namespace PlayerViewRefreshPolicy;

\tassert(ShouldRender(false, 0.0f, false));
\tassert(ShouldRender(false, 0.0f, true));
\tassert(ShouldRender(true, 0.0f, false));
\tassert(!ShouldRender(true, 0.0f, true));
\tassert(!ShouldRender(true, IdleRefreshIntervalSeconds - 0.01f, true));
\tassert(ShouldRender(true, IdleRefreshIntervalSeconds, true));
\treturn 0;
}
'''
save("Tests/PlayerViewRefreshPolicySmokeTest.cpp", test)


path = "Docs/ECS_Scheduler_Migration_Plan.md"
text = load(path)
text = replace_once(
    text,
    "- `Docs/Step16G_D3D11_Real_Triangle_Completion.md`\n",
    "- `Docs/Step16G_D3D11_Real_Triangle_Completion.md`\n"
    "- `Docs/Step16G_Player_View_Regression_Guard.md`\n",
    "Player View regression guard document link",
)
save(path, text)


doc = '''# Step 16-G: Player View Regression Guard

## 自動化した回帰防止

- 停止・一時停止中でも出力SRVが未生成なら初回描画を必ず実行する
- Player Viewの利用可能サイズを`SceneManagerContext::PlayerScreenSize`へ同期する
- 全ての早期returnでMain Render Targetを復元する
- 出力SRVがnullの場合は`ImGui::Image`へ渡さず診断表示へ切り替える
- 非表示・折り畳み状態では不要なPlayer描画を行わない
- Fullscreen経路でもPlayer画面サイズをContextへ同期する

## 自動検証

`PlayerViewRefreshPolicySmokeTest`で次を確認する。

- Playing / Stepでは毎フレーム描画
- Stopped / Pausedでも出力未生成なら即時描画
- 出力生成後のStopped / Pausedは1秒間隔に制限

## 実機確認が必要な項目

- Editor ViewとPlayer Viewを同時表示した状態で両方が正常に更新される
- Player ViewのResize後にアスペクト比と描画領域が一致する
- Play / Pause / Stop / Step遷移後も映像が残る
- PostEffectとOverlay UIがPlayer Viewへ表示される
- Player Viewを閉じたPlaying状態でMain Windowへ描画される

実機確認が完了するまでMigration Planの`既存Player View実機回帰`は未完了のまま維持する。
'''
save("Docs/Step16G_Player_View_Regression_Guard.md", doc)

print("Player View regression guard migration applied")
