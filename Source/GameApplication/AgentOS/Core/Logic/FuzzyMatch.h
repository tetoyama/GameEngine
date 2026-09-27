// =======================================================================
//
// FuzzyMatch.h
//
// Entity名 / Component名のあいまい一致（Fuzzy Match）を行う純粋ロジック。
// エンジンヘッダに依存しないportableな実装で、g++でも単体テスト可能。
// 「Player」という入力がEntity名の厳密一致とは限らない、
// あるいはComponent名の部分一致（例:「Light」→「LightComponent」）で
// 解決したいというAgentOSの要求に応えるための基礎スコアリングを提供する。
//
// =======================================================================
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace agentos::fuzzy {

// ASCII範囲のみを小文字化する。
// Entity名/Component名は基本ASCIIだが、日本語名が混ざっても
// マルチバイト文字を破壊しないようにASCII判定のみ行う。
inline std::string ToLowerAscii(std::string text) {
	for(char& ch : text) {
		if(ch >= 'A' && ch <= 'Z') {
			ch = static_cast<char>(ch - 'A' + 'a');
		}
	}
	return text;
}

// 1件のFuzzy Match結果。
// matchType:
//   "exact"             ... 完全一致
//   "case_insensitive"  ... 大文字小文字無視の完全一致
//   "substring"         ... candidateがqueryを部分文字列として含む
//   "reverse_substring" ... queryがcandidateを部分文字列として含む
struct Match {
	std::string candidate;
	std::string matchType;
	double score = 0.0;
};

namespace detail {

struct ScoredMatchType {
	double score = 0.0;
	std::string matchType;
};

// query/candidateの一致度を判定し、スコアとmatchTypeをまとめて返す内部ヘルパー。
// スコアの優先順位: exact(1.0) > case_insensitive(0.95) >
// substring(最大0.8) > reverse_substring(最大0.6) > 不一致(0.0)
inline ScoredMatchType ScoreNameDetailed(
	const std::string& query,
	const std::string& candidate
) {
	if(query.empty() || candidate.empty()) return {0.0, {}};

	if(query == candidate) return {1.0, "exact"};

	const std::string lowerQuery = ToLowerAscii(query);
	const std::string lowerCandidate = ToLowerAscii(candidate);

	if(lowerQuery == lowerCandidate) return {0.95, "case_insensitive"};

	// candidateがqueryを含む（例: query="Play" candidate="Player"）。
	// 一致率(query長/candidate長)が高いほど信頼度を上げる。0.3未満には切り下げない。
	if(lowerCandidate.find(lowerQuery) != std::string::npos) {
		const double ratio =
			static_cast<double>(query.size()) / static_cast<double>(candidate.size());
		const double clamped = std::clamp(ratio, 0.3, 1.0);
		return {0.8 * clamped, "substring"};
	}

	// queryがcandidateを含む（例: query="MainPlayerEntity" candidate="Player"）。
	// substringより信頼度を下げる（query側に余分な情報が多いほど誤爆しやすいため）。
	if(lowerQuery.find(lowerCandidate) != std::string::npos) {
		const double ratio =
			static_cast<double>(candidate.size()) / static_cast<double>(query.size());
		const double clamped = std::clamp(ratio, 0.3, 1.0);
		return {0.6 * clamped, "reverse_substring"};
	}

	return {0.0, {}};
}

} // namespace detail

// query が candidate とどれだけ一致するかを [0,1] のスコアで返す。
inline double ScoreName(const std::string& query, const std::string& candidate) {
	return detail::ScoreNameDetailed(query, candidate).score;
}

// ScoreNameのmatchType込み版。呼び出し側でEntity等の付随情報と
// 紐付けたままFuzzy Matchの内訳（どの一致種別だったか）を残したい場合に使う。
inline Match ScoreNameMatch(const std::string& query, const std::string& candidate) {
	const detail::ScoredMatchType scored = detail::ScoreNameDetailed(query, candidate);
	return Match{candidate, scored.matchType, scored.score};
}

// candidatesをqueryとのスコアで降順ソートし、上位maxResults件を返す。
// スコアが同点の場合はcandidate文字列の辞書順（昇順）で安定させる。
// minScore未満の候補は結果から除外する。
inline std::vector<Match> RankCandidates(
	const std::string& query,
	const std::vector<std::string>& candidates,
	std::size_t maxResults,
	double minScore = 0.3
) {
	std::vector<Match> matches;
	matches.reserve(candidates.size());
	for(const std::string& candidate : candidates) {
		const detail::ScoredMatchType scored =
			detail::ScoreNameDetailed(query, candidate);
		if(scored.score < minScore) continue;
		matches.push_back(Match{candidate, scored.matchType, scored.score});
	}

	std::stable_sort(
		matches.begin(),
		matches.end(),
		[](const Match& lhs, const Match& rhs) {
			if(lhs.score != rhs.score) return lhs.score > rhs.score;
			return lhs.candidate < rhs.candidate;
		}
	);

	if(matches.size() > maxResults) matches.resize(maxResults);
	return matches;
}

// Component名向けのFuzzy Match。
// 「Light」-> 「LightComponent」のように、"Component"接尾辞の有無を
// 吸収した上でScoreNameの最良値（matchType込み）を返す。
// 試行する組み合わせ:
//   1. query そのもの                 vs componentName
//   2. query + "Component"            vs componentName （例: "Light" -> "LightComponent"）
//   3. queryの"Component"接尾辞を外したもの vs componentName （例: "LightComponent" -> "Light"）
inline Match ScoreComponentNameMatch(
	const std::string& query,
	const std::string& componentName
) {
	constexpr const char* kSuffix = "Component";
	constexpr std::size_t kSuffixLength = 9;

	detail::ScoredMatchType best = detail::ScoreNameDetailed(query, componentName);

	const detail::ScoredMatchType withSuffix =
		detail::ScoreNameDetailed(query + kSuffix, componentName);
	if(withSuffix.score > best.score) best = withSuffix;

	if(query.size() > kSuffixLength &&
		query.compare(query.size() - kSuffixLength, kSuffixLength, kSuffix) == 0) {
		const std::string stripped = query.substr(0, query.size() - kSuffixLength);
		const detail::ScoredMatchType withoutSuffix =
			detail::ScoreNameDetailed(stripped, componentName);
		if(withoutSuffix.score > best.score) best = withoutSuffix;
	}

	return Match{componentName, best.matchType, best.score};
}

// ScoreComponentNameMatchのスコア部分のみを返す簡易版。
inline double ScoreComponentName(
	const std::string& query,
	const std::string& componentName
) {
	return ScoreComponentNameMatch(query, componentName).score;
}

// componentNamesをqueryとのScoreComponentNameスコアで降順ソートし、
// 上位maxResults件を返す（同点はcandidate辞書順、minScore未満は除外）。
// RankCandidatesのComponent名版。
inline std::vector<Match> RankComponentCandidates(
	const std::string& query,
	const std::vector<std::string>& componentNames,
	std::size_t maxResults,
	double minScore = 0.3
) {
	std::vector<Match> matches;
	matches.reserve(componentNames.size());
	for(const std::string& componentName : componentNames) {
		Match match = ScoreComponentNameMatch(query, componentName);
		if(match.score < minScore) continue;
		matches.push_back(std::move(match));
	}

	std::stable_sort(
		matches.begin(),
		matches.end(),
		[](const Match& lhs, const Match& rhs) {
			if(lhs.score != rhs.score) return lhs.score > rhs.score;
			return lhs.candidate < rhs.candidate;
		}
	);

	if(matches.size() > maxResults) matches.resize(maxResults);
	return matches;
}

} // namespace agentos::fuzzy
