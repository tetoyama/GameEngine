// =======================================================================
//
// SystemIntrospection.h
//
// SystemRegistryが保持するSystemTaskとコンパイル済みScheduleをJSON化する
// （構想§11: RuntimeTrace/CodeSearchと並ぶEvidence源）。
//
// =======================================================================
#pragma once

#include "../Core/Json.h"

class SystemRegistry;
class ComponentRegistry;
struct SceneContext;

namespace agentos {

// SystemRegistry::GetTasks() の全SystemTaskを配列化し、componentReads/componentWrites は
// ComponentRegistry::GetComponentIDByTypeIndex()経由でtype_index -> 登録名へ変換する。
// ComponentRegistryへ登録されていない型（Resource等）は "raw:<type_index.name()>" として出力する。
//
// edgesはFixed/Frame/Editor/Renderの4Domain分、SystemScheduleCompilerが構築した
// Access競合由来の依存辺をTask名のペアとして出力する。
// 明示的なBefore/After宣言は現状のエンジンに存在しないため対象外
// （SystemScheduleCompiler.h参照。依存辺はAccess競合からのみ導出される）。
Json ExportSystemDescriptors(SystemRegistry& registry, ComponentRegistry& components);

// SceneContextから直接呼び出す簡易オーバーロード。
// system/componentのいずれかが無い場合は {"error":...} を返す。
Json ExportSystemDescriptors(SceneContext& context);

} // namespace agentos
