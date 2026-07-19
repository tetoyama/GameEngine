// =======================================================================
//
// RetrievalWorker.h
//
// 割り当てられたTaskを遂行するためにTool呼び出しを提案・実行するWorker担当
// （構想§9）。結論（原因の断定）は一切行わず、Evidence列と失敗report（Toolの
// 失敗も含む）を返す。
//
// SchemaRejectedからの自己修復リトライ:
// GenerateQueriesが提案したCommand（LLMの自由記述）がCommandPipelineへ
// SchemaRejectedで拒否された場合に限り、拒否理由と正しいargumentSchemaを
// LLMへ渡して1件だけ修正させ、再提出する。Task全体で最大2回まで。
// Explicit/Deterministic Commandは対象外（既にGrounding済みのため）。
// 修復にも失敗した場合は従来どおりToolError Evidenceを記録してbreakする。
//
// =======================================================================
#pragma once

#include <vector>

#include "AgentContext.h"
#include "../Evidence/Evidence.h"

namespace agentos {

class RetrievalWorker {
public:
	// storeTaskId: TaskStore上のTask ID（Evidence.taskId・Command監査に使う）。
	// taskSpec: Planner出力中の1タスク分（type/allowedTools/searchHints等を含む）。
	// evidenceOutへ収集したEvidence（成功・失敗どちらも）を追加する。
	// summaryOutが非nullなら {executed, failed, coverageNote} を格納する。
	static Result Run(AgentContext& ctx, TaskId storeTaskId, const Json& taskSpec,
	                   std::vector<Evidence>* evidenceOut, Json* summaryOut = nullptr);
};

} // namespace agentos
