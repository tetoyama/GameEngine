// =======================================================================
//
// LogicGraph.cpp
//
// =======================================================================
#include "LogicGraph.h"

#include <algorithm>
#include <cmath>

namespace agentos {

LogicNodeId LogicGraph::AddHypothesis(const std::string& text, double rubricBase) {
	Node node;
	node.id = nextId_++;
	node.text = text;
	node.rubricBase = rubricBase;
	nodes_.push_back(std::move(node));
	return nodes_.back().id;
}

LogicGraph::Node* LogicGraph::FindNodeMutable(LogicNodeId node) {
	for (Node& n : nodes_) {
		if (n.id == node) {
			return &n;
		}
	}
	return nullptr;
}

const LogicGraph::Node* LogicGraph::FindNode(LogicNodeId node) const {
	for (const Node& n : nodes_) {
		if (n.id == node) {
			return &n;
		}
	}
	return nullptr;
}

void LogicGraph::AddSupport(LogicNodeId node, EvidenceId evidence) {
	if (Node* n = FindNodeMutable(node)) {
		n->supports.push_back(evidence);
	}
}

void LogicGraph::AddContradiction(LogicNodeId node, EvidenceId evidence) {
	if (Node* n = FindNodeMutable(node)) {
		n->contradicts.push_back(evidence);
	}
}

void LogicGraph::AddMissingEvidence(LogicNodeId node, const std::string& description) {
	if (Node* n = FindNodeMutable(node)) {
		n->missingEvidence.push_back(description);
	}
}

double LogicGraph::ComputeConfidence(LogicNodeId node) const {
	const Node* n = FindNode(node);
	if (n == nullptr) {
		return 0.0;
	}
	const std::size_t supportCount = n->supports.size();
	if (supportCount == 0) {
		return 0.0;
	}
	const std::size_t contradictionCount = n->contradicts.size();

	const double supportFactor = 1.0 - std::pow(0.6, static_cast<double>(supportCount));
	const double contradictionFactor = std::pow(0.5, static_cast<double>(contradictionCount));
	double confidence = n->rubricBase * supportFactor * contradictionFactor;

	// clamp01
	if (confidence < 0.0) {
		confidence = 0.0;
	}
	if (confidence > 1.0) {
		confidence = 1.0;
	}
	return confidence;
}

std::vector<LogicGraph::RankedHypothesis> LogicGraph::Rank() const {
	std::vector<RankedHypothesis> ranked;
	ranked.reserve(nodes_.size());
	for (const Node& n : nodes_) {
		RankedHypothesis r;
		r.id = n.id;
		r.text = n.text;
		r.confidence = ComputeConfidence(n.id);
		r.supports = n.supports;
		r.contradicts = n.contradicts;
		r.missingEvidence = n.missingEvidence;
		ranked.push_back(std::move(r));
	}

	// confidence降順、安定ソート（同値は追加順を維持）。
	std::stable_sort(ranked.begin(), ranked.end(),
		[](const RankedHypothesis& a, const RankedHypothesis& b) {
			return a.confidence > b.confidence;
		});

	return ranked;
}

Json LogicGraph::ToJson() const {
	Json arr = Json::array();
	for (const RankedHypothesis& r : Rank()) {
		Json supports = Json::array();
		for (const EvidenceId e : r.supports) {
			supports.push_back(e);
		}
		Json contradicts = Json::array();
		for (const EvidenceId e : r.contradicts) {
			contradicts.push_back(e);
		}
		Json missing = Json::array();
		for (const std::string& m : r.missingEvidence) {
			missing.push_back(m);
		}
		arr.push_back(Json::object({
			{"id", r.id},
			{"text", r.text},
			{"confidence", r.confidence},
			{"supports", std::move(supports)},
			{"contradicts", std::move(contradicts)},
			{"missingEvidence", std::move(missing)},
		}));
	}
	Json j = Json::object();
	j["hypotheses"] = std::move(arr);
	return j;
}

} // namespace agentos
