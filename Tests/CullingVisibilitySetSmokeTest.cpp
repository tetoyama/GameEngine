#include <cassert>
#include "Engine/Scene/System/Render/Culling/CullingVisibilitySet.h"

int main(){
	CullingVisibilitySet visibility;
	const Entity camera{10, 2};
	const CullingViewKey editorView{1, camera, CullingViewKind::Editor, 0};
	const CullingViewKey playerView{1, camera, CullingViewKind::Player, 0};
	const CullingViewKey shadowView{1, camera, CullingViewKind::Shadow, 2};
	const Entity model{20, 4};
	const Entity terrain{21, 1};

	visibility.BeginFrame(42);
	visibility.BeginView(editorView);
	visibility.ReserveView(editorView, 16);
	assert(visibility.SetVisible(editorView, model));
	assert(visibility.SetVisible(editorView, terrain));
	visibility.BeginView(playerView);
	assert(visibility.SetVisible(playerView, terrain));
	visibility.BeginView(shadowView);
	assert(visibility.SetVisible(shadowView, model));

	assert(visibility.ViewCount() == 3);
	assert(visibility.IsVisible(editorView, model));
	assert(!visibility.IsVisible(playerView, model));
	assert(visibility.IsVisible(shadowView, model));
	assert(visibility.PeakVisibleCount(1) == 2);
	assert(visibility.GrowthEventCount(1) > 0);

	// RenderSystemのconst accessor経由でも、論理的にmutableなTelemetryだけを
	// リセットできる契約を固定する。
	const CullingVisibilitySet& readOnlyVisibility = visibility;
	readOnlyVisibility.ResetPeakMetrics(1);
	assert(readOnlyVisibility.PeakVisibleCount(1) == 2);
	assert(readOnlyVisibility.GrowthEventCount(1) == 0);

	// Growth禁止時に部分的な可視集合を公開すると誤Cullingになるため、
	// 容量不足が発生したView全体を無効化して安全Fallbackへ戻す。
	const CullingViewKey strictView{2, camera, CullingViewKind::Player, 0};
	visibility.BeginView(strictView, false);
	visibility.ReserveView(strictView, 1);

	bool rejected = false;
	for(uint32_t index = 0; index < 64; ++index){
		if(!visibility.SetVisible(strictView, Entity{100 + index, 1})){
			rejected = true;
			break;
		}
	}
	assert(rejected);
	assert(visibility.IsViewOverflowed(strictView));
	assert(!visibility.HasView(strictView));
	assert(visibility.VisibleCount(strictView) == 0);
	assert(visibility.GrowthEventCount(2) == 1);

	visibility.ClearView(playerView);
	assert(visibility.ViewCount() == 2);
	visibility.BeginFrame(43);
	assert(visibility.ViewCount() == 0);
	return 0;
}
