// =======================================================================
//
// AgentOSFuzzyMatchSmokeTest.cpp
//
// FuzzyMatch.h（あいまいEntity/Component名解決）と、ComponentQueryRouting.h
// のなりすまし防止（属性フレーズbail out）を検証するportable smoke test。
// エンジンヘッダには一切依存しない（g++ -std=c++20 -Wall -Wextra -Werror でビルド可）。
//
// =======================================================================
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "AgentOS/Core/Logic/ComponentQueryRouting.h"
#include "AgentOS/Core/Logic/FuzzyMatch.h"

using namespace agentos;

namespace {

bool NearlyEqual(double lhs, double rhs, double epsilon = 1e-9) {
	return std::fabs(lhs - rhs) < epsilon;
}

void TestScoreNameOrdering() {
	// exact
	assert(NearlyEqual(fuzzy::ScoreName("Player", "Player"), 1.0));

	// case_insensitive
	assert(NearlyEqual(fuzzy::ScoreName("player", "Player"), 0.95));
	assert(NearlyEqual(fuzzy::ScoreName("PLAYER", "Player"), 0.95));

	// substring（candidateがqueryを含む）
	const double substringScore = fuzzy::ScoreName("Play", "Player");
	assert(substringScore > 0.0 && substringScore < 0.95);
	assert(NearlyEqual(substringScore, 0.8 * (4.0 / 6.0)));

	// reverse_substring（queryがcandidateを含む）
	const double reverseScore = fuzzy::ScoreName("PlayerEntityFoo", "Player");
	assert(reverseScore > 0.0);
	assert(NearlyEqual(reverseScore, 0.6 * (6.0 / 15.0)));

	// 不一致
	assert(NearlyEqual(fuzzy::ScoreName("Zzz", "Player"), 0.0));

	// 順序: exact > case_insensitive > substring > reverse_substring > 不一致
	assert(fuzzy::ScoreName("Player", "Player") > fuzzy::ScoreName("player", "Player"));
	assert(fuzzy::ScoreName("player", "Player") > substringScore);
	assert(substringScore > reverseScore);
	assert(reverseScore > fuzzy::ScoreName("Zzz", "Player"));

	// matchType確認
	assert(fuzzy::ScoreNameMatch("Player", "Player").matchType == "exact");
	assert(fuzzy::ScoreNameMatch("player", "Player").matchType == "case_insensitive");
	assert(fuzzy::ScoreNameMatch("Play", "Player").matchType == "substring");
	assert(fuzzy::ScoreNameMatch("PlayerEntityFoo", "Player").matchType == "reverse_substring");
	assert(fuzzy::ScoreNameMatch("Zzz", "Player").matchType.empty());
}

void TestScoreComponentNameLightToLightComponent() {
	// 「Light」-> 「LightComponent」の部分一致解決（"Component"接尾辞の吸収）。
	const double score = fuzzy::ScoreComponentName("Light", "LightComponent");
	assert(NearlyEqual(score, 1.0));
	assert(fuzzy::ScoreComponentNameMatch("Light", "LightComponent").matchType == "exact");

	// 接尾辞つきqueryが接尾辞なしcandidateへも解決できる（逆方向）。
	const double reverseScore = fuzzy::ScoreComponentName("LightComponent", "Light");
	assert(NearlyEqual(reverseScore, 1.0));

	// 無関係なComponent名はヒットしない。
	assert(NearlyEqual(fuzzy::ScoreComponentName("Light", "TransformComponent"), 0.0));

	// 大文字小文字違いでも接尾辞吸収後にcase_insensitiveへ落ちる。
	const double ciScore = fuzzy::ScoreComponentName("light", "LightComponent");
	assert(NearlyEqual(ciScore, 0.95));
}

void TestRankCandidates() {
	const std::vector<std::string> candidates = {"Alpha", "alpha", "Beta"};

	// 既定minScore(0.3)では"Beta"は完全に無関係なので除外される。
	const std::vector<fuzzy::Match> ranked = fuzzy::RankCandidates("Alpha", candidates, 10);
	assert(ranked.size() == 2);
	assert(ranked[0].candidate == "Alpha");
	assert(NearlyEqual(ranked[0].score, 1.0));
	assert(ranked[1].candidate == "alpha");
	assert(NearlyEqual(ranked[1].score, 0.95));

	// maxResultsで打ち切られる。
	const std::vector<fuzzy::Match> limited = fuzzy::RankCandidates("Alpha", candidates, 1);
	assert(limited.size() == 1);
	assert(limited[0].candidate == "Alpha");

	// minScoreを引き上げるとcase_insensitive一致も除外される。
	const std::vector<fuzzy::Match> strict = fuzzy::RankCandidates("Alpha", candidates, 10, 0.99);
	assert(strict.size() == 1);
	assert(strict[0].candidate == "Alpha");

	// 同点はcandidateのアルファベット順（昇順）で安定する。
	const std::vector<std::string> tieCandidates = {"PlayerTwo", "PlayerOne"};
	const std::vector<fuzzy::Match> tied = fuzzy::RankCandidates("Player", tieCandidates, 10);
	assert(tied.size() == 2);
	assert(NearlyEqual(tied[0].score, tied[1].score));
	assert(tied[0].candidate == "PlayerOne");
	assert(tied[1].candidate == "PlayerTwo");
}

void TestRankComponentCandidates() {
	const std::vector<std::string> componentNames = {
		"LightComponent", "TransformComponent", "NameComponent"};
	const std::vector<fuzzy::Match> ranked =
		fuzzy::RankComponentCandidates("Light", componentNames, 5);
	assert(!ranked.empty());
	assert(ranked[0].candidate == "LightComponent");
	assert(NearlyEqual(ranked[0].score, 1.0));
}

// -----------------------------------------------------------------------
// ComponentQueryRouting narrowing
// -----------------------------------------------------------------------

void TestAttributePhraseBailOut() {
	// 「Playerのジャンプ力を教えて」は属性フレーズであり、Entity名の確定Routeを
	// 誤爆させてはならない（Fuzzy解決はOrchestrator/Tool経路に委ねる）。
	const auto route = componentquery::Resolve("Playerのジャンプ力を教えて");
	assert(!route.IsValid());
}

void TestExistingRoutesStillWork() {
	// AgentOSComponentQueryRoutingSmokeTestの契約（既存ケース）を壊していないことを確認する。
	{
		const auto route = componentquery::Resolve("Fieldになんのコンポーネントがあるの？");
		assert(route.tool == "DescribeEntity");
		assert(route.arguments.at("entityName") == "Field");
	}
	{
		const auto route = componentquery::Resolve("Entity 'Light' にも？");
		assert(route.tool == "DescribeEntity");
		assert(route.arguments.at("entityName") == "Light");
	}
	{
		const auto route = componentquery::Resolve("LightComponentの設定を教えて");
		assert(route.tool == "ReadComponent");
		assert(route.arguments.at("entityName") == "Light");
		assert(route.arguments.at("component") == "LightComponent");
	}
	{
		const auto route = componentquery::Resolve("ライトコンポーネントとかもない状態？");
		assert(route.tool == "ReadComponent");
		assert(route.arguments.at("entityName") == "Light");
		assert(route.arguments.at("component") == "LightComponent");
	}
	{
		const auto route = componentquery::Resolve("PlayerのTransformComponentの設定を教えて");
		assert(route.tool == "ReadComponent");
		assert(route.arguments.at("entityName") == "Player");
		assert(route.arguments.at("component") == "TransformComponent");
	}
	{
		// Component名だけからEntityを捏造しない。
		const auto route = componentquery::Resolve("TransformComponentの設定を教えて");
		assert(!route.IsValid());
	}
	{
		// Scene全体の要求はScene Snapshot経路へ渡す（ここでは短絡させない）。
		const auto route = componentquery::Resolve("今のシーンの状況を教えて");
		assert(!route.IsValid());
	}
}

} // namespace

int main() {
	TestScoreNameOrdering();
	TestScoreComponentNameLightToLightComponent();
	TestRankCandidates();
	TestRankComponentCandidates();
	TestAttributePhraseBailOut();
	TestExistingRoutesStillWork();
	std::cout << "AgentOSFuzzyMatchSmokeTest: PASS\n";
	return 0;
}
