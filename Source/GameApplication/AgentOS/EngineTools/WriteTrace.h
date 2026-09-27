// =======================================================================
//
// WriteTrace.h
//
// Frame差分トレーサ v1（構想§11）。
// 対象Entity/Componentを毎フレームサンプリングし、前回サンプルとの差分が
// あれば記録する。差分検出時は、そのComponent型へ書き込み宣言している
// SystemTaskを「疑わしい書き込み元」として付随情報付きで列挙する
// （実際の書き込みを直接観測しているわけではないため attribution="estimated"）。
//
// Sample()はSceneへアクセスするため必ずMain Thread（AgentOSPanel::Draw経由）
// から呼び出すこと。SetTarget/Clear/Stop/GetTraceもMainThreadDispatcher経由の
// Tool実行から呼ばれる想定であり、本クラス自体は排他制御を持たない
// （呼び出しが常にMain Thread上で直列に行われる契約のため）。
//
// =======================================================================
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

#include "../Core/Json.h"
#include "Scene/Entity/Entity.h"
#include "Scene/Interface/IComponentStorage.h" // ComponentTypeID

struct SceneContext;

namespace agentos {

class WriteTracer {
public:
	// トレース対象を設定し、トレースを有効化する。
	// 前回サンプルは破棄され、次回Sample()から新規に差分検出を始める。
	void SetTarget(Entity entity, std::string componentName, SceneContext* sceneContext);

	// 蓄積済みイベントと対象設定をすべて破棄する。
	void Clear();

	// サンプリングのみ停止する（蓄積済みイベントはGetTrace()で取得可能なまま残す）。
	void Stop();

	bool IsActive() const noexcept { return m_active; }

	// 対象Componentを1回サンプリングする。前回サンプルとの差分があれば記録する。
	void Sample(std::int64_t frame);

	// 記録済みイベントの配列を返す。
	Json GetTrace() const;

private:
	static constexpr std::size_t kMaxEvents = 4096;

	Json CollectSuspectedWriters(ComponentTypeID targetTypeID) const;

	bool m_active = false;
	Entity m_entity{};
	std::string m_componentName;
	SceneContext* m_sceneContext = nullptr;

	bool m_hasPrevious = false;
	Json m_previous;

	std::deque<Json> m_events;
};

} // namespace agentos
