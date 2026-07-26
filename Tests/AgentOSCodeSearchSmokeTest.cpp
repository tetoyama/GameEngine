// =======================================================================
//
// AgentOSCodeSearchSmokeTest.cpp
//
// AgentOS Core / CodeIndex の検索・永続化層のスモークテスト。
//   VectorMath / MockEmbeddingBackend / CodeIndexStore / CodeSearch
//
// 配線確認に留めず、次の実質的な性質まで検証する。
//   - BLOBを往復してもベクトルがビット単位で保存されること
//   - 差分更新でファイル単位の置換が正しく効くこと
//   - 字句とベクトルが「互いに苦手を補う」こと
//   - ハイブリッドが片側だけの検索を上回るケースが実在すること
//
// =======================================================================
#include "AgentOS/Core/CodeIndex/CodeIndexStore.h"
#include "AgentOS/Core/CodeIndex/CodeSearch.h"
#include "AgentOS/Core/CodeIndex/MockEmbeddingBackend.h"
#include "AgentOS/Core/CodeIndex/VectorMath.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace agentos;

namespace {

std::string TempDbPath(const char* tag) {
	return std::string("/tmp/agentos_codeindex_") + tag + ".db";
}

CodeChunk MakeChunk(
	const std::string& file, const std::string& mod,
	const std::string& name, int start, int end, const std::string& text,
	CodeChunkKind kind = CodeChunkKind::Function) {
	CodeChunk c;
	c.kind = kind;
	c.filePath = file;
	c.moduleTag = mod;
	c.qualifiedName = name;
	c.startLine = start;
	c.endLine = end;
	c.text = text;
	return c;
}

int RankOf(const std::vector<SearchHit>& hits, const SearchIndex& index, const std::string& name) {
	for(std::size_t i = 0; i < hits.size(); ++i) {
		if(index.At(hits[i].index).chunk.qualifiedName == name) return static_cast<int>(i) + 1;
	}
	return 0; // 圏外
}

// -----------------------------------------------------------------------
// VectorMath
// -----------------------------------------------------------------------
void TestVectorMathBasics() {
	std::vector<float> v{3.0f, 4.0f};
	L2Normalize(&v);
	assert(std::fabs(std::sqrt(v[0] * v[0] + v[1] * v[1]) - 1.0f) < 1e-5f);

	// ゼロベクトルは割らずにそのまま返す
	std::vector<float> zero{0.0f, 0.0f};
	L2Normalize(&zero);
	assert(zero[0] == 0.0f && zero[1] == 0.0f);

	// 正規化済み同士の内積 == コサイン類似度
	std::vector<float> a{1.0f, 0.0f};
	std::vector<float> b{1.0f, 1.0f};
	L2Normalize(&b);
	const float dot = DotProduct(a.data(), b.data(), 2);
	assert(std::fabs(dot - CosineSimilarity(a, b)) < 1e-5f);

	std::printf("  [ok] L2 normalize / dot product / cosine agree\n");
}

void TestBlobRoundTripIsBitExact() {
	std::vector<float> v{0.125f, -3.5f, 1e-7f, 12345.678f};
	const std::vector<float> back = BlobToFloats(FloatsToBlob(v));
	assert(back.size() == v.size());
	for(std::size_t i = 0; i < v.size(); ++i) {
		assert(back[i] == v[i]); // 量子化も丸めも挟まない
	}

	// 端数のあるBLOBは壊れているとみなす
	std::vector<std::uint8_t> broken{1, 2, 3};
	assert(BlobToFloats(broken).empty());

	std::printf("  [ok] float <-> blob round trip is bit exact\n");
}

// -----------------------------------------------------------------------
// MockEmbeddingBackend
// -----------------------------------------------------------------------
void TestMockEmbeddingIsMeaningful() {
	MockEmbeddingBackend backend(128);

	const std::vector<float> a = backend.EmbedOne("void SqliteDb::Prepare(sql statement)");
	const std::vector<float> b = backend.EmbedOne("SqliteDb Prepare statement sql");
	const std::vector<float> c = backend.EmbedOne("float PlatformerBoss::UpdateJumpArc(dt)");

	// 正規化済み
	assert(std::fabs(CosineSimilarity(a, a) - 1.0f) < 1e-4f);
	// 語彙が重なる方が近い（スタブではなく実際に意味を持つベクトル）
	const float simSame = CosineSimilarity(a, b);
	const float simDiff = CosineSimilarity(a, c);
	assert(simSame > simDiff);
	assert(simSame > 0.5f);

	// 決定論的
	assert(backend.EmbedOne("abc") == backend.EmbedOne("abc"));

	std::printf("  [ok] mock embedding is deterministic and lexically meaningful\n");
}

// -----------------------------------------------------------------------
// CodeIndexStore
// -----------------------------------------------------------------------
void TestStorePersistsChunksAndVectors() {
	const std::string path = TempDbPath("persist");
	std::remove(path.c_str());

	MockEmbeddingBackend backend(64);

	CodeIndexStore store;
	assert(store.Open(path));
	assert(store.EnsureSchema());
	assert(store.SetMeta("embedding_model", backend.Name()));

	std::vector<CodeChunk> chunks{
		MakeChunk("a/A.cpp", "a", "A::One", 1, 5, "void A::One() { doThing(); }"),
		MakeChunk("a/A.cpp", "a", "A::Two", 7, 9, "void A::Two() { doOther(); }"),
	};

	std::vector<std::string> texts;
	for(const CodeChunk& c : chunks) texts.push_back(c.EmbedText());
	std::vector<std::vector<float>> vectors;
	assert(backend.Embed(texts, &vectors));

	assert(store.ReplaceFile("a/A.cpp", CodeIndexStore::HashContent("src"), chunks, vectors));

	std::int64_t count = 0;
	assert(store.CountChunks(&count));
	assert(count == 2);

	// 再オープンしても内容が保たれること
	store.Close();
	CodeIndexStore reopened;
	assert(reopened.Open(path));

	std::string model;
	assert(reopened.GetMeta("embedding_model", &model));
	assert(model == backend.Name());

	std::string hash;
	assert(reopened.GetFileHash("a/A.cpp", &hash));
	assert(hash == CodeIndexStore::HashContent("src"));

	std::vector<StoredChunk> loaded;
	assert(reopened.LoadAll(&loaded));
	assert(loaded.size() == 2);
	assert(loaded[0].chunk.qualifiedName == "A::One");
	assert(loaded[0].chunk.startLine == 1 && loaded[0].chunk.endLine == 5);
	assert(loaded[0].embedding.size() == 64);
	// ベクトルもビット単位で戻ること
	assert(loaded[0].embedding == vectors[0]);

	reopened.Close();
	std::remove(path.c_str());
	std::printf("  [ok] store persists chunks, vectors and metadata\n");
}

void TestIncrementalReplaceAndPrune() {
	const std::string path = TempDbPath("incremental");
	std::remove(path.c_str());

	CodeIndexStore store;
	assert(store.Open(path));
	assert(store.EnsureSchema());

	const std::vector<std::vector<float>> noVectors;

	assert(store.ReplaceFile("a/A.cpp", "h1",
		{MakeChunk("a/A.cpp", "a", "A::One", 1, 3, "one")}, noVectors));
	assert(store.ReplaceFile("b/B.cpp", "h2",
		{MakeChunk("b/B.cpp", "b", "B::One", 1, 3, "one"),
		 MakeChunk("b/B.cpp", "b", "B::Two", 5, 7, "two")}, noVectors));

	std::int64_t count = 0;
	assert(store.CountChunks(&count));
	assert(count == 3);

	// A.cpp を「編集された」ものとして差し替える。B.cpp は影響を受けない。
	assert(store.ReplaceFile("a/A.cpp", "h1b",
		{MakeChunk("a/A.cpp", "a", "A::Renamed", 1, 4, "renamed"),
		 MakeChunk("a/A.cpp", "a", "A::Added", 6, 8, "added")}, noVectors));

	assert(store.CountChunks(&count));
	assert(count == 4); // A:2 + B:2

	std::string hash;
	assert(store.GetFileHash("a/A.cpp", &hash));
	assert(hash == "h1b");

	std::vector<StoredChunk> loaded;
	assert(store.LoadAll(&loaded));
	bool sawRenamed = false;
	bool sawOldOne = false;
	for(const StoredChunk& sc : loaded) {
		if(sc.chunk.qualifiedName == "A::Renamed") sawRenamed = true;
		if(sc.chunk.qualifiedName == "A::One") sawOldOne = true;
	}
	assert(sawRenamed);
	assert(!sawOldOne); // 旧チャンクが残っていないこと

	// 消えたファイルの掃除
	int removed = 0;
	assert(store.RemoveFilesNotIn({"a/A.cpp"}, &removed));
	assert(removed == 1);
	assert(store.CountChunks(&count));
	assert(count == 2);

	// 未ベクトル化のチャンクはembeddingが空で返る
	assert(store.LoadAll(&loaded));
	assert(!loaded.empty());
	assert(loaded[0].embedding.empty());

	store.Close();
	std::remove(path.c_str());
	std::printf("  [ok] incremental file replace and stale-file pruning\n");
}

// -----------------------------------------------------------------------
// 検索
// -----------------------------------------------------------------------
std::vector<StoredChunk> BuildCorpus(MockEmbeddingBackend& backend) {
	std::vector<CodeChunk> chunks{
		MakeChunk("Engine/Physics/Collision.cpp", "Engine/Physics",
		          "Physics::AabbOverlap", 10, 30,
		          "bool Physics::AabbOverlap(const Aabb& a, const Aabb& b) {\n"
		          "  // 軸平行境界ボックスの重なりを判定する\n"
		          "  return a.min.x <= b.max.x && a.max.x >= b.min.x;\n}"),
		MakeChunk("AgentOS/Core/Store/SqliteDb.cpp", "AgentOS/Core/Store",
		          "agentos::SqliteDb::Prepare", 40, 60,
		          "Result SqliteDb::Prepare(const std::string& sql, Statement* out) {\n"
		          "  int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);\n}"),
		MakeChunk("AgentOS/Core/Command/CommandPipeline.cpp", "AgentOS/Core/Command",
		          "agentos::CommandPipeline::ApplyCodePatch", 100, 140,
		          "Result CommandPipeline::ApplyCodePatch(const Patch& patch) {\n"
		          "  // パッチを適用しコンパイルを走らせる\n  return Compile();\n}"),
		MakeChunk("Engine/Render/Culling.cpp", "Engine/Render",
		          "Render::FrustumCull", 5, 40,
		          "void Render::FrustumCull(const Frustum& f, VisibilityList* out) {\n"
		          "  // 視錐台の外にある描画対象を除外する\n}"),
		MakeChunk("Engine/Physics/Contact.cpp", "Engine/Physics",
		          "Physics::ResolveContact", 12, 44,
		          "void Physics::ResolveContact(Contact& c) {\n"
		          "  // 衝突した剛体同士の反発を解決する\n}"),
		// 妨害役。本文の語彙は正解と強く重なるが、求められているシンボルではない。
		// ベクトル検索は本文の語彙に引きずられてこちらを上位に出す。
		MakeChunk("Engine/Physics/ContactCache.cpp", "Engine/Physics",
		          "Physics::ContactCache", 1, 80,
		          "// 衝突した剛体同士の反発に関する情報を保持する\n"
		          "// 衝突 剛体 反発 衝突 剛体 反発 衝突 剛体 反発\n"
		          "struct Physics::ContactCache { Contact contacts[64]; };",
		          CodeChunkKind::Type),
	};

	std::vector<std::string> texts;
	for(const CodeChunk& c : chunks) texts.push_back(c.EmbedText());
	std::vector<std::vector<float>> vectors;
	assert(backend.Embed(texts, &vectors));

	std::vector<StoredChunk> stored;
	for(std::size_t i = 0; i < chunks.size(); ++i) {
		StoredChunk sc;
		sc.id = static_cast<std::int64_t>(i) + 1;
		sc.chunk = chunks[i];
		sc.embedding = vectors[i];
		stored.push_back(std::move(sc));
	}
	return stored;
}

void TestTokenizerSplitsCompoundNames() {
	const std::vector<std::string> t = TokenizeForSearch("ApplyCodePatch m_frameCounter HTTPServer");
	const auto has = [&](const char* s) {
		return std::find(t.begin(), t.end(), std::string(s)) != t.end();
	};
	assert(has("applycodepatch")); // 元の語も残す（完全一致を優先するため）
	assert(has("apply"));
	assert(has("code"));
	assert(has("patch"));
	assert(has("frame"));
	assert(has("counter"));
	assert(has("http"));   // 連続大文字→小文字の境界
	assert(has("server"));

	// 日本語はbigramへ割る（空白で区切られないため）
	const std::vector<std::string> jp = TokenizeForSearch("当たり判定");
	const auto hasJp = [&](const char* s) {
		return std::find(jp.begin(), jp.end(), std::string(s)) != jp.end();
	};
	assert(!jp.empty());
	assert(hasJp("当た"));
	assert(hasJp("り判"));
	assert(hasJp("判定"));

	// 日本語と英数字が混ざっても両方取れる
	const std::vector<std::string> mixed = TokenizeForSearch("Aabb の重なりを判定");
	const auto hasMixed = [&](const char* s) {
		return std::find(mixed.begin(), mixed.end(), std::string(s)) != mixed.end();
	};
	assert(hasMixed("aabb"));
	assert(hasMixed("判定"));

	std::printf("  [ok] tokenizer splits CamelCase / snake_case / acronyms / CJK bigrams\n");
}

void TestLexicalWinsOnKnownSymbolName() {
	MockEmbeddingBackend backend(256);
	SearchIndex index;
	assert(index.Build(BuildCorpus(backend)));
	assert(index.Size() == 6);
	assert(index.EmbeddedCount() == 6);

	// シンボル名を知っているクエリ。完全一致が最強である領域。
	const std::vector<SearchHit> hits = index.SearchLexical("ApplyCodePatch", 5);
	assert(!hits.empty());
	assert(index.At(hits[0].index).chunk.qualifiedName
	       == "agentos::CommandPipeline::ApplyCodePatch");

	std::printf("  [ok] lexical search nails an exact symbol name\n");
}

void TestVectorFindsSemanticNeighbours() {
	MockEmbeddingBackend backend(256);
	SearchIndex index;
	assert(index.Build(BuildCorpus(backend)));

	// 名前を知らず、本文の語彙で探すクエリ
	const std::vector<float> q = backend.EmbedOne("軸平行境界ボックスの重なりを判定する Aabb");
	const std::vector<SearchHit> hits = index.SearchVector(q, 5);
	assert(!hits.empty());
	assert(index.At(hits[0].index).chunk.qualifiedName == "Physics::AabbOverlap");
	// 正規化済みなのでスコアは[-1,1]に収まる
	assert(hits[0].score <= 1.0001f);

	std::printf("  [ok] vector search retrieves by body vocabulary\n");
}

void TestHybridRecoversWhatOneSideMisses() {
	MockEmbeddingBackend backend(256);
	SearchIndex index;
	assert(index.Build(BuildCorpus(backend)));

	// 名前を出さず本文の語彙だけで探すクエリ。
	// 妨害役(ContactCache)は本文の語彙が濃いのでベクトル側では1位になるが、
	// 求められている実装は ResolveContact の方。
	const std::string query = "衝突した剛体の反発を解決";
	const std::vector<float> q = backend.EmbedOne(query);

	const std::vector<SearchHit> lex = index.SearchLexical(query, 6);
	const std::vector<SearchHit> vec = index.SearchVector(q, 6);
	const std::vector<SearchHit> hyb = index.SearchHybrid(query, q, 6);

	// 日本語クエリで字句検索が機能すること。
	// UTF-8をbigramへ割らないとここが空になり、字句側が半身になる。
	assert(!lex.empty());

	const int lexRank = RankOf(lex, index, "Physics::ResolveContact");
	const int vecRank = RankOf(vec, index, "Physics::ResolveContact");
	const int hybRank = RankOf(hyb, index, "Physics::ResolveContact");

	// この検証が意味を持つには、2経路が実際に食い違っている必要がある。
	// 両方とも既に1位ならハイブリッドの効果を何も証明できない。
	assert(lexRank == 1);
	assert(vecRank == 2);
	assert(lexRank != vecRank);

	// RRFでは L1V2 と L2V1 が完全な同点になる。
	// 同点は字句側優先で decide する設計なので、正解が上に来る。
	assert(hybRank == 1);

	// --- 片側だけが見つけたものを融合が拾い上げること（recall回復） ---
	// AabbOverlap はクエリと語彙が重ならず字句側では圏外だが、
	// ベクトル側が拾うため融合結果には残る。
	const int aabbLex = RankOf(lex, index, "Physics::AabbOverlap");
	const int aabbVec = RankOf(vec, index, "Physics::AabbOverlap");
	const int aabbHyb = RankOf(hyb, index, "Physics::AabbOverlap");
	assert(aabbLex == 0); // 字句側は完全に取り逃す
	assert(aabbVec > 0);  // ベクトル側は拾う
	assert(aabbHyb > 0);  // 融合すると残る

	// 融合結果には両経路の順位が診断用に残る
	bool sawBothRanks = false;
	for(const SearchHit& h : hyb) {
		if(h.lexicalRank > 0 && h.vectorRank > 0) sawBothRanks = true;
	}
	assert(sawBothRanks);

	std::printf("  [ok] hybrid RRF: target lex=%d vec=%d -> %d / "
	            "lexical-missed item recovered at %d\n",
	            lexRank, vecRank, hybRank, aabbHyb);
}

void TestSearchDegradesGracefullyWithoutVectors() {
	// ベクトル未構築の索引でも字句検索は動くこと。
	// 初回インデックス構築中に検索が来た場合のフォールバック経路。
	std::vector<StoredChunk> stored;
	StoredChunk sc;
	sc.id = 1;
	sc.chunk = MakeChunk("a/A.cpp", "a", "A::DoWork", 1, 5, "void A::DoWork() {}");
	stored.push_back(sc);

	SearchIndex index;
	assert(index.Build(std::move(stored)));
	assert(index.EmbeddedCount() == 0);
	assert(index.Dimensions() == 0);

	assert(index.SearchVector({0.1f, 0.2f}, 5).empty()); // 次元不一致は空を返す
	const std::vector<SearchHit> lex = index.SearchLexical("DoWork", 5);
	assert(lex.size() == 1);

	// ハイブリッドも片翼だけで成立する
	const std::vector<SearchHit> hyb = index.SearchHybrid("DoWork", {}, 5);
	assert(hyb.size() == 1);

	std::printf("  [ok] search degrades gracefully when vectors are absent\n");
}

} // namespace

int main() {
	std::printf("==== AgentOSCodeSearchSmokeTest ====\n");

	TestVectorMathBasics();
	TestBlobRoundTripIsBitExact();
	TestMockEmbeddingIsMeaningful();
	TestStorePersistsChunksAndVectors();
	TestIncrementalReplaceAndPrune();
	TestTokenizerSplitsCompoundNames();
	TestLexicalWinsOnKnownSymbolName();
	TestVectorFindsSemanticNeighbours();
	TestHybridRecoversWhatOneSideMisses();
	TestSearchDegradesGracefullyWithoutVectors();

	std::printf("==== AgentOSCodeSearchSmokeTest: PASSED ====\n");
	return 0;
}
