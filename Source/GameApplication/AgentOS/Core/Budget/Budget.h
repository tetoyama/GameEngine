// =======================================================================
//
// Budget.h
//
// Task/Session単位のリソース上限管理（構想§7）。
// SupervisorはBudgetTrackerの消費結果のみを見て打ち切りを判断する。
//
// =======================================================================
#pragma once

#include <cstdint>
#include <mutex>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

// ---------------------------------
// Budget（上限定義。値そのものは不変データ）
// ---------------------------------
struct Budget {
	// Repair回数を固定値で狭く制限せず、大規模調査を進展がある限り継続できる
	// セッション予算にする。停滞時はEarlyStoppingが先に止める。
	int maxToolCalls = 250;
	int maxLlmCalls = 120;
	int maxRetries = 2;
	int maxDepth = 3;
	std::int64_t maxLlmChars = 4000000;
	std::int64_t maxMillis = 21600000; // 6時間。CPU推論の長時間調査を許容する。
	int maxModifiedFiles = 5;
};

// ---------------------------------
// BudgetTracker
// スレッドセーフ。各Consume*は上限超過時にResult::Failを返し、内部カウンタは
// 増加させない（超過状態のまま呼び続けても値が発散しない）。
// ---------------------------------
class BudgetTracker {
public:
	explicit BudgetTracker(Budget budget = {});

	Result ConsumeToolCall();
	Result ConsumeLlmCall(std::int64_t chars);
	Result ConsumeRetry();
	Result ConsumeModifiedFile();
	Result CheckDepth(int depth) const;

	void AddElapsedMillis(std::int64_t millis);

	// 各次元（toolCalls/llmCalls/llmChars/millis）の残り比率のうち最小値を返す（0..1）。
	double RemainingRatio() const;

	// 使用量/上限を次元ごとに列挙したJSONを返す（UI表示・Evidence用）。
	Json ToJson() const;

private:
	mutable std::mutex mutex_;
	Budget budget_;

	int usedToolCalls_ = 0;
	int usedLlmCalls_ = 0;
	int usedRetries_ = 0;
	int usedModifiedFiles_ = 0;
	std::int64_t usedLlmChars_ = 0;
	std::int64_t usedMillis_ = 0;
};

} // namespace agentos
