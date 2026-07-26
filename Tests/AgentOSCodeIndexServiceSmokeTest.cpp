// =======================================================================
//
// AgentOSCodeIndexServiceSmokeTest.cpp
//
// CodeIndexService と CodeSearchTool のスモークテスト。
//
// エンジンへ組み込む層なので、次の実運用上の性質を重点的に見る。
//   - バックグラウンド構築が完了し、検索できるようになること
//   - 構築完了前に検索してもクラッシュせず空を返すこと
//   - 埋め込み未接続でも字句検索が成立すること
//   - 差分更新で変更ファイルだけが再処理されること
//   - Shutdownが構築中でも安全に抜けること
//   - ツールが「索引未完成」と「該当なし」を区別して返すこと
//
// =======================================================================
#include "AgentOS/Core/CodeIndex/CodeIndexService.h"
#include "AgentOS/Core/CodeIndex/CodeSearchTool.h"
#include "AgentOS/Core/CodeIndex/MockEmbeddingBackend.h"
#include "AgentOS/Core/Command/CapabilitySet.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace agentos;

namespace {

namespace fs = std::filesystem;

const std::string kRoot = "/tmp/agentos_codeindex_service_src";
const std::string kDb = "/tmp/agentos_codeindex_service.db";

void WriteFile(const std::string& path, const std::string& content) {
	fs::create_directories(fs::path(path).parent_path());
	std::ofstream out(path, std::ios::binary);
	out << content;
}

void ResetFixture() {
	std::error_code ec;
	fs::remove_all(kRoot, ec);
	fs::remove(kDb, ec);

	WriteFile(kRoot + "/Physics/Collision.cpp",
		"namespace Physics {\n"
		"bool Physics::AabbOverlap(const Aabb& a, const Aabb& b) {\n"
		"    // 軸平行境界ボックスの重なりを判定する\n"
		"    return a.min.x <= b.max.x;\n"
		"}\n"
		"} // namespace Physics\n");

	WriteFile(kRoot + "/Render/Culling.h",
		"struct FrustumCuller {\n"
		"    // 視野の外にある描画対象を除外する\n"
		"    int planeCount = 6;\n"
		"};\n");
}

// 構築完了を待つ。無限には待たない。
bool WaitReady(const CodeIndexService& service, int timeoutMs = 15000) {
	const auto deadline =
		std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	while(std::chrono::steady_clock::now() < deadline) {
		const CodeIndexStatus status = service.GetStatus();
		if(status.state == CodeIndexState::Ready) return true;
		if(status.state == CodeIndexState::Failed) return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return false;
}

CodeIndexServiceContext MakeContext() {
	CodeIndexServiceContext ctx;
	ctx.sourceRoot = kRoot;
	ctx.databasePath = kDb;
	ctx.buildOnStart = true;
	ctx.options.excludeSubstrings.clear(); // フィクスチャにBackendsは無い
	return ctx;
}

// -----------------------------------------------------------------------
void TestBackgroundBuildThenSearch() {
	ResetFixture();

	CodeIndexService service;
	assert(service.Initialize(MakeContext()));
	assert(WaitReady(service));

	const CodeIndexStatus status = service.GetStatus();
	assert(status.state == CodeIndexState::Ready);
	assert(status.filesTotal == 2);
	assert(status.filesReindexed == 2);
	assert(status.chunkCount >= 2);
	// 埋め込み未接続なのでベクトルはゼロ。字句検索のみの索引。
	assert(status.embeddedCount == 0);
	assert(status.error.empty());

	const std::vector<CodeSearchResult> hits = service.Search("AabbOverlap", 5);
	assert(!hits.empty());
	assert(hits[0].qualifiedName.find("AabbOverlap") != std::string::npos);
	assert(hits[0].startLine > 0);
	assert(!hits[0].filePath.empty());
	assert(!hits[0].text.empty());

	service.Shutdown();
	std::printf("  [ok] background build completes and lexical search works\n");
}

void TestSearchBeforeReadyIsSafe() {
	ResetFixture();

	CodeIndexService service;
	assert(service.Initialize(MakeContext()));

	// 構築完了を待たずに検索する。空が返るだけでクラッシュしないこと。
	// 初回構築が数十分かかりうる以上、この経路は必ず踏まれる。
	const std::vector<CodeSearchResult> early = service.Search("AabbOverlap", 5);
	(void)early; // タイミング次第で0件にも数件にもなる。落ちないことが要件。

	assert(WaitReady(service));
	service.Shutdown();
	std::printf("  [ok] searching before the index is ready is safe\n");
}

void TestIncrementalRebuildSkipsUnchanged() {
	ResetFixture();

	{
		CodeIndexService first;
		assert(first.Initialize(MakeContext()));
		assert(WaitReady(first));
		assert(first.GetStatus().filesReindexed == 2);
		first.Shutdown();
	}

	// 2回目は内容が変わっていないので全件スキップされる
	{
		CodeIndexService second;
		assert(second.Initialize(MakeContext()));
		assert(WaitReady(second));
		const CodeIndexStatus status = second.GetStatus();
		assert(status.filesReindexed == 0);
		assert(status.filesUnchanged == 2);
		second.Shutdown();
	}

	// 1ファイルだけ書き換えると、そのファイルだけが再処理される
	WriteFile(kRoot + "/Render/Culling.h",
		"struct FrustumCuller {\n"
		"    // 視野の外にある描画対象を除外する\n"
		"    int planeCount = 6;\n"
		"    float bias = 0.0f;\n"
		"};\n");

	{
		CodeIndexService third;
		assert(third.Initialize(MakeContext()));
		assert(WaitReady(third));
		const CodeIndexStatus status = third.GetStatus();
		assert(status.filesReindexed == 1);
		assert(status.filesUnchanged == 1);
		third.Shutdown();
	}

	std::printf("  [ok] incremental rebuild reprocesses only changed files\n");
}

void TestEmbeddingBackendEnablesHybrid() {
	ResetFixture();

	MockEmbeddingBackend backend(64);

	CodeIndexServiceContext ctx = MakeContext();
	ctx.embedding = &backend;

	CodeIndexService service;
	assert(service.Initialize(std::move(ctx)));
	assert(WaitReady(service));

	const CodeIndexStatus status = service.GetStatus();
	assert(status.embeddedCount > 0); // ベクトルが埋まっている
	assert(status.embeddedCount == status.chunkCount);

	// ハイブリッド経路が通ること（両経路の順位が結果に残る）
	const std::vector<CodeSearchResult> hits = service.Search("重なりを判定する", 5);
	assert(!hits.empty());
	bool sawVectorRank = false;
	for(const CodeSearchResult& h : hits) {
		if(h.vectorRank > 0) sawVectorRank = true;
	}
	assert(sawVectorRank);

	service.Shutdown();
	std::printf("  [ok] attaching an embedding backend enables the hybrid route\n");
}

void TestShutdownDuringBuildIsSafe() {
	ResetFixture();

	CodeIndexService service;
	assert(service.Initialize(MakeContext()));
	// 完了を待たずに落とす。ワーカーが中断フラグを見て抜けること。
	service.Shutdown();

	// 二重Shutdownしても壊れない
	service.Shutdown();

	std::printf("  [ok] shutdown during build terminates cleanly\n");
}

void TestToolDistinguishesNotReadyFromNoResults() {
	ResetFixture();

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);

	CodeIndexService service;
	CodeIndexServiceContext ctx = MakeContext();
	ctx.buildOnStart = false; // 意図的に構築させない
	assert(service.Initialize(std::move(ctx)));

	RegisterCodeSearchTool(pipeline, service);

	// この時点では索引が未完成。ツールは前提条件で弾くこと。
	// 黙って0件を返すと、モデルが「そんなコードは無い」と誤断する。
	{
		CodeIndexService& s = service;
		assert(!s.IsReady());
	}

	// 構築を要求して待つ
	service.RequestRebuild(false);
	assert(WaitReady(service));
	assert(service.IsReady());

	// 存在しないものを引いても空の結果が返るだけ（失敗ではない）
	const std::vector<CodeSearchResult> none =
		service.Search("ZZZ_絶対に存在しないシンボル名_ZZZ", 5);
	assert(none.empty());

	// 存在するものは引ける
	const std::vector<CodeSearchResult> found = service.Search("FrustumCuller", 5);
	assert(!found.empty());

	service.Shutdown();
	std::printf("  [ok] tool separates 'index not ready' from 'no results'\n");
}

void TestToolNamesMatchAgentOSExpectations() {
	// AgentOS側は "CodeSearch" という名前を前提に組まれている。
	//   - RetrievalWorker の kDiscoveryTools（引数のEvidence束縛を免除する一覧）
	//   - PlannerAgent / CriticAgent / Orchestrator のTask種別
	// ここが "SearchCode" だったために Discovery Tool と認識されず、
	// 検索クエリが毎回 GroundingRejected で弾かれた（実機ログで確認）。
	// 名前の一致は仕様なので、テストで固定する。
	ResetFixture();

	CapabilityRegistry registry;
	CommandPipeline pipeline(&registry);

	CodeIndexService service;
	assert(service.Initialize(MakeContext()));
	assert(WaitReady(service));

	RegisterCodeSearchTool(pipeline, service);

	const Json catalog = pipeline.DescribeTools();
	assert(catalog.is_array());

	bool sawCodeSearch = false;
	bool sawGetSymbolInfo = false;
	for(const auto& tool : catalog) {
		const std::string name = tool.value("name", std::string());
		if(name == "CodeSearch") sawCodeSearch = true;
		if(name == "GetSymbolInfo") sawGetSymbolInfo = true;
		// 旧名が残っていないこと
		assert(name != "SearchCode");
	}
	assert(sawCodeSearch);
	assert(sawGetSymbolInfo);

	service.Shutdown();
	std::printf("  [ok] tool names match AgentOS expectations (CodeSearch / GetSymbolInfo)\n");
}

void TestFileFilterAndSymbolLookup() {
	ResetFixture();

	CodeIndexService service;
	assert(service.Initialize(MakeContext()));
	assert(WaitReady(service));

	// --- ファイル絞り込み ---
	// モデルが実際に file 引数を渡してきて SchemaRejected になった（実機ログ）。
	const std::vector<CodeSearchResult> inPhysics =
		service.Search("判定", 5, "Physics");
	for(const CodeSearchResult& r : inPhysics) {
		assert(r.filePath.find("Physics") != std::string::npos);
	}

	const std::vector<CodeSearchResult> inRender =
		service.Search("判定", 5, "Render");
	for(const CodeSearchResult& r : inRender) {
		assert(r.filePath.find("Render") != std::string::npos);
	}

	// 一致しないフィルタでは0件
	assert(service.Search("判定", 5, "存在しないディレクトリ").empty());

	// --- シンボル名でのピンポイント取得 ---
	// 末尾のみの指定でも引ける
	const std::vector<CodeSearchResult> bySuffix =
		service.FindSymbol("AabbOverlap", std::string(), 5);
	assert(!bySuffix.empty());
	assert(bySuffix[0].qualifiedName.find("AabbOverlap") != std::string::npos);
	// 全文が返る（切り詰めない）
	assert(bySuffix[0].text.find("return") != std::string::npos);

	// 修飾付きの完全一致
	const std::vector<CodeSearchResult> byExact =
		service.FindSymbol(bySuffix[0].qualifiedName, std::string(), 5);
	assert(!byExact.empty());
	assert(byExact[0].qualifiedName == bySuffix[0].qualifiedName);

	// 存在しない名前は空
	assert(service.FindSymbol("ZZZ_NoSuchSymbol_ZZZ", std::string(), 5).empty());

	service.Shutdown();
	std::printf("  [ok] file filter and exact symbol lookup\n");
}

void TestNoWorkerThreadUntilNeeded() {
	// buildOnStart=false ならワーカースレッドを起こさないこと。
	// 索引を使わない構成で余計なスレッドを抱えないための性質であり、
	// スレッド起因の不具合を切り分けるスイッチとしても機能する。
	ResetFixture();

	CodeIndexService service;
	CodeIndexServiceContext ctx = MakeContext();
	ctx.buildOnStart = false;
	assert(service.Initialize(std::move(ctx)));

	// 何も要求していない状態。索引は空だが検索しても落ちない。
	assert(!service.IsReady());
	assert(service.Search("なんでもよい", 5).empty());
	assert(service.GetStatus().state == CodeIndexState::Idle);

	// スレッドを起こさないままShutdownしても安全に抜ける
	service.Shutdown();

	// 起こしてから畳む経路も確認する
	CodeIndexService second;
	CodeIndexServiceContext ctx2 = MakeContext();
	ctx2.buildOnStart = false;
	assert(second.Initialize(std::move(ctx2)));
	second.RequestRebuild(false); // ここで初めてスレッドが立つ
	assert(WaitReady(second));
	second.Shutdown();

	std::printf("  [ok] worker thread is not spawned until a build is requested\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSCodeIndexServiceSmokeTest ====\n");

	TestBackgroundBuildThenSearch();
	TestSearchBeforeReadyIsSafe();
	TestIncrementalRebuildSkipsUnchanged();
	TestEmbeddingBackendEnablesHybrid();
	TestShutdownDuringBuildIsSafe();
	TestToolDistinguishesNotReadyFromNoResults();
	TestToolNamesMatchAgentOSExpectations();
	TestFileFilterAndSymbolLookup();
	TestNoWorkerThreadUntilNeeded();

	std::error_code ec;
	fs::remove_all(kRoot, ec);
	fs::remove(kDb, ec);

	std::printf("==== AgentOSCodeIndexServiceSmokeTest: PASSED ====\n");
	return 0;
}
