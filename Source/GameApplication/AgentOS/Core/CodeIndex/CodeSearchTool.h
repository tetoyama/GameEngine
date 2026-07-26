// =======================================================================
//
// CodeSearchTool.h
//
// CodeIndexService を CommandPipeline のツールとして公開する。
//
// 位置づけ：
//   EngineTools（EntityIntrospection / SystemIntrospection）が
//   「実行中のシーンを引く手」なら、これは「ソースコードを引く手」。
//   同じ ICommandExecutor として登録されるので、Orchestrator から見れば
//   他のretrievalツールと区別が無く、収集結果はそのまま Evidence に入る。
//
//   RAGを独立したサブシステムとして作るのではなく、既存の retrieval 経路に
//   バックエンドを1本足す形にしている
//   （Docs/AgentOS/04_Execution_Engine_Roadmap.md のRAG下層/上層の分離）。
//
// =======================================================================
#pragma once

#include "../Command/CommandPipeline.h"
#include "CodeIndexService.h"

namespace agentos {

// CodeIndexService を参照する2つのツールを pipeline へ登録する。
//
//   CodeSearch    : 曖昧検索。名前を知らなくても引ける。抜粋を返す。
//   GetSymbolInfo : 名前指定のピンポイント取得。定義の全文を返す。
//
// ツール名 "CodeSearch" は RetrievalWorker の kDiscoveryTools および
// PlannerAgent のTask種別と一致させる必要がある（CodeSearchTool.cpp 冒頭の注記参照）。
//
// service は呼び出し側が所有し、pipeline より長く生存させること。
void RegisterCodeSearchTool(CommandPipeline& pipeline, CodeIndexService& service);

} // namespace agentos
