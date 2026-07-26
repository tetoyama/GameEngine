// =======================================================================
//
// AgentOSTaskRetirementSmokeTest.cpp
//
// 再計画による「Taskの撤回」のスモークテスト。
//
// 背景（実機で踏んだ故障）:
//   静的なコードの問い（SqliteDb::Prepareの実装を見せて）に対して、
//   Plannerが誤って実行時トレースTaskを立て、StartWriteTraceが
//   「entity not found」で失敗した。答えそのものはCodeSearchが正しく
//   取得できていたにもかかわらず、その失敗1件が
//     coverage < 1.0 / tasksWithoutEvidence > 0 / failedEvidenceCount > 0
//   の3ゲートを同時に踏み、セッションは「未完了」で終わった。
//
//   これらは累積した履歴で計算されるため、修復Taskをいくら追加しても
//   分母が増えるだけでpassへ戻れない（passへ到達する経路が存在しない）。
//   原因は再計画が「追加」しかできず「撤回」ができなかったこと。
//
// ここで検証するのは次の2点。
//   1. 撤回により3つの指標が実際に回復すること
//   2. 撤回が濫用できないこと（有用なEvidenceは消せない）
//
// =======================================================================
#include "AgentOS/Core/Evidence/Evidence.h"
#include "AgentOS/Core/Evidence/EvidenceBuilder.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

using namespace agentos;

namespace {

Evidence MakeUsable(TaskId taskId, const std::string& claim) {
	Evidence e;
	e.taskId = taskId;
	e.claim = claim;
	e.payload = Json::object({{"requestRevision", 0}, {"value", claim}});
	e.provenance.sourceType = "Tool:CodeSearch";
	e.provenance.sourceUri = "CodeSearch";
	return e;
}

Evidence MakeFailure(TaskId taskId, const std::string& error) {
	Evidence e;
	e.taskId = taskId;
	e.claim = "Tool StartWriteTrace returned an error result";
	e.payload = Json::object({
		{"requestRevision", 0},
		{"failure", true},
		{"error", error},
	});
	e.provenance.sourceType = "ToolResultError";
	e.provenance.sourceUri = "StartWriteTrace";
	return e;
}

bool Contains(const std::vector<TaskId>& v, TaskId t) {
	return std::find(v.begin(), v.end(), t) != v.end();
}

// -----------------------------------------------------------------------
// 実機の故障を再現し、撤回で回復することを示す
// -----------------------------------------------------------------------
void TestRetirementRecoversAllThreeGates() {
	EvidenceBuilder builder;

	// task 1: CodeSearch が正解を取得（目的は達成されている）
	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeUsable(1, "agentos::SqliteDb::Prepare の実装を取得した"));

	// task 2: 静的な問いに対して誤って立った実行時トレース
	builder.MarkPlannedTask(2, 0);
	builder.Add(MakeFailure(2, "entity not found: agentos::SqliteDb::Prepare"));

	// --- 撤回前: 3ゲートすべてが不合格の状態 ---
	{
		const EvidenceBuilder::BuiltEvidence built = builder.Build();
		assert(built.coverage < 1.0);              // ゲート#1
		assert(!built.tasksWithoutEvidence.empty()); // ゲート#2
		assert(built.failedEvidenceCount > 0);       // ゲート#4
		assert(built.usableEvidenceCount == 1);
		assert(built.retiredTasks.empty());

		// 修復Taskを足しても分母が増えるだけでcoverageは1.0へ戻らない、
		// というのが元の詰みの構造。ここで実際にそうなることを示す。
		EvidenceBuilder probe = builder;
		probe.MarkPlannedTask(3, 0);
		probe.Add(MakeUsable(3, "追加調査は成功した"));
		const EvidenceBuilder::BuiltEvidence afterAdd = probe.Build();
		assert(afterAdd.coverage < 1.0);            // 2/3。1.0には届かない
		assert(afterAdd.failedEvidenceCount > 0);   // 失敗は消えない
	}

	// --- 撤回: task 2 は立てるべきでなかった ---
	builder.RequestRetireTask(2);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();

	assert(Contains(built.retiredTasks, 2));
	assert(built.coverage == 1.0);                 // ゲート#1 回復
	assert(built.tasksWithoutEvidence.empty());    // ゲート#2 回復
	assert(built.failedEvidenceCount == 0);        // ゲート#4 回復

	// 正解のEvidenceは残っていること（撤回は失敗だけを外す）
	assert(built.usableEvidenceCount == 1);
	assert(built.evidences.size() == 1);
	assert(built.evidences[0].taskId == 1);

	std::printf("  [ok] retirement recovers coverage / tasksWithoutEvidence / failedEvidenceCount\n");
}

// -----------------------------------------------------------------------
// 濫用できないこと
// -----------------------------------------------------------------------
void TestUsefulTaskCannotBeRetired() {
	// 撤回の判断はLLMが行うため、誤判断で有用な観測が消えないよう
	// プログラム側が拒否できなければならない。
	EvidenceBuilder builder;

	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeUsable(1, "重要な観測結果"));

	builder.RequestRetireTask(1); // 有用なEvidenceを持つTaskの撤回を要求

	const EvidenceBuilder::BuiltEvidence built = builder.Build();

	// 要求は却下される
	assert(built.retiredTasks.empty());
	assert(built.usableEvidenceCount == 1);
	assert(built.evidences.size() == 1);
	assert(built.coverage == 1.0);

	std::printf("  [ok] a task that produced usable evidence cannot be retired\n");
}

void TestPartiallySuccessfulTaskCannotBeRetired() {
	// 成功と失敗が混在するTaskも撤回できない。
	// 「必要だったが一部失敗した」Taskを消すと本当の欠落が隠れるため。
	EvidenceBuilder builder;

	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeUsable(1, "片方は取れた"));
	builder.Add(MakeFailure(1, "もう片方は取れなかった"));

	builder.RequestRetireTask(1);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();

	assert(built.retiredTasks.empty());
	assert(built.failedEvidenceCount == 1); // 失敗は隠されない
	assert(built.usableEvidenceCount == 1);

	std::printf("  [ok] a partially successful task cannot be retired\n");
}

void TestRetiringEveryTaskIsStillCoherent() {
	// 全Taskが撤回された場合でも破綻しないこと。
	// activePlannedTasksが空になるとcoverageは1.0になる仕様なので、
	// ここは「目的が満たされたか」を見る別のゲート（#9）に委ねられる。
	EvidenceBuilder builder;
	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeFailure(1, "駄目だった"));
	builder.RequestRetireTask(1);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();
	assert(Contains(built.retiredTasks, 1));
	assert(built.evidences.empty());
	assert(built.usableEvidenceCount == 0);   // ゲート#3が拾う
	assert(built.failedEvidenceCount == 0);

	std::printf("  [ok] retiring every task leaves a coherent (and still failing) state\n");
}

void TestUnknownTaskRetirementIsHarmless() {
	// 存在しないtaskIdの撤回要求（LLMの誤りうる出力）で壊れないこと
	EvidenceBuilder builder;
	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeUsable(1, "結果"));

	builder.RequestRetireTask(999);

	const EvidenceBuilder::BuiltEvidence built = builder.Build();
	// 該当Taskが無い＝有用Evidenceも無いので撤回自体は成立するが、
	// 計画にもEvidenceにも影響しない。
	assert(built.coverage == 1.0);
	assert(built.usableEvidenceCount == 1);
	assert(built.evidences.size() == 1);

	std::printf("  [ok] retiring an unknown task id is harmless\n");
}

void TestRetirementIsReportedForAudit() {
	// 何が計画から外れたかはtranscriptに残る必要がある。
	// 撤回は「証拠を消す」操作なので、監査できないと危険。
	EvidenceBuilder builder;
	builder.MarkPlannedTask(1, 0);
	builder.Add(MakeUsable(1, "結果"));
	builder.MarkPlannedTask(2, 0);
	builder.Add(MakeFailure(2, "不要だった観測"));
	builder.RequestRetireTask(2);

	const Json json = EvidenceBuilder::ToJson(builder.Build());
	assert(json.contains("retiredTasks"));
	assert(json.at("retiredTasks").is_array());
	assert(json.at("retiredTasks").size() == 1);
	assert(json.at("retiredTasks")[0].get<std::int64_t>() == 2);

	std::printf("  [ok] retirements are recorded in the built evidence json\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSTaskRetirementSmokeTest ====\n");

	TestRetirementRecoversAllThreeGates();
	TestUsefulTaskCannotBeRetired();
	TestPartiallySuccessfulTaskCannotBeRetired();
	TestRetiringEveryTaskIsStillCoherent();
	TestUnknownTaskRetirementIsHarmless();
	TestRetirementIsReportedForAudit();

	std::printf("==== AgentOSTaskRetirementSmokeTest: PASSED ====\n");
	return 0;
}
