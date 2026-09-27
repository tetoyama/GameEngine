// =======================================================================
//
// LogicGraph.h
//
// Evidenceから組み立てた仮説（Hypothesis）のグラフ（構想§5.5）。
// LogicNodeはsupports/contradictsでEvidenceIdを参照する。
// Confidenceは決定的な式で算出し、LLMの自己申告値は採用しない。
//
// =======================================================================
#pragma once

#include <string>
#include <vector>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

class LogicGraph {
public:
	// 仮説を追加する。idはグラフ内ローカルで1から始まる連番。
	// rubricBaseは論拠の質の自己評価（0..1）であり、最終confidenceそのものではない。
	LogicNodeId AddHypothesis(const std::string& text, double rubricBase);

	void AddSupport(LogicNodeId node, EvidenceId evidence);
	void AddContradiction(LogicNodeId node, EvidenceId evidence);
	void AddMissingEvidence(LogicNodeId node, const std::string& description);

	// Confidence算出式（決定的）:
	//   supportCount == 0 の場合 confidence = 0
	//   それ以外は
	//     confidence = clamp01( rubricBase * (1 - 0.6^supportCount) * 0.5^contradictionCount )
	// 直感: supportが増えるほど (1 - 0.6^n) は1へ漸近（ただしrubricBaseを超えない）。
	//       contradictionが1件あるごとに半減させる。
	double ComputeConfidence(LogicNodeId node) const;

	struct RankedHypothesis {
		LogicNodeId id = kInvalidId;
		std::string text;
		double confidence = 0.0;
		std::vector<EvidenceId> supports;
		std::vector<EvidenceId> contradicts;
		std::vector<std::string> missingEvidence;
	};

	// confidence降順。同値の場合は追加順（安定ソート）を維持する。
	std::vector<RankedHypothesis> Rank() const;

	Json ToJson() const;

private:
	struct Node {
		LogicNodeId id = kInvalidId;
		std::string text;
		double rubricBase = 0.0;
		std::vector<EvidenceId> supports;
		std::vector<EvidenceId> contradicts;
		std::vector<std::string> missingEvidence;
	};

	const Node* FindNode(LogicNodeId node) const;
	Node* FindNodeMutable(LogicNodeId node);

	std::vector<Node> nodes_;
	LogicNodeId nextId_ = 1;
};

} // namespace agentos
