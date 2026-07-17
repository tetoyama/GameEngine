// =======================================================================
//
// TaskDataflowResolver.h
//
// Plannerの論理Task IDとEngine Toolの実引数を分離し、依存Taskの構造化出力から
// Tool引数を決定的に束縛する。LLMが"T1"等をEntity名として出力しても、そのまま
// Engineへ渡さない。
//
// =======================================================================
#pragma once

#include <cstddef>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

class TaskStore;

struct WorkerCommandResolution {
	Json commands = Json::array();
	Json rejections = Json::array();
	int strippedArguments = 0;
	int resolvedReferences = 0;
};

// taskSpec.dependenciesが参照する先行Taskについて、TaskStoreのresult/Evidenceを
// planTaskId単位にまとめる。internalTaskIdsにはSession内の全planTaskIdを格納する。
Json BuildTaskDependencyContext(TaskStore& store, SessionId sessionId, const Json& taskSpec);

// Workerが提案したcommandsを、allowedTools由来のcatalogと依存出力に照らして検証する。
// - catalog外Toolを拒否
// - Tool schemaに無い引数を除去
// - dependency task IDを実データへ束縛
// - current/非依存task IDのEngine識別子への混入を拒否
// - foreach展開はmaxCommands件で打ち切る
WorkerCommandResolution ResolveWorkerCommands(
	const Json& rawCommands,
	const Json& taskSpec,
	const Json& filteredToolCatalog,
	const Json& dependencyContext,
	std::size_t maxCommands = 5);

} // namespace agentos
