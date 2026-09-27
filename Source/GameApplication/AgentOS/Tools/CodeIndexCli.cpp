// =======================================================================
//
// CodeIndexCli.cpp
//
// コード索引を構築するオフラインCLI。
//
// 位置づけ：
//   索引構築のロジックそのものは AgentOS/Core/CodeIndex にあり、
//   このCLIとエディタ内のボタンの両方から呼べる。CLIを先に用意するのは、
//   MSVC実ビルドを待たずにパーサの挙動を実測で確認できるようにするため。
//
// 使い方:
//   codeindex [--root DIR] [--out FILE.json] [--exclude SUBSTR]...
//             [--top N] [--quiet]
//
// =======================================================================
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <chrono>

#include "AgentOS/Core/CodeIndex/CodeIndexBuilder.h"
#include "AgentOS/Core/CodeIndex/CodeIndexStore.h"
#include "AgentOS/Core/CodeIndex/CodeSearch.h"
#include "AgentOS/Core/CodeIndex/MockEmbeddingBackend.h"

using namespace agentos;

namespace {

void PrintUsage() {
	std::printf(
		"usage: codeindex [options]\n"
		"  --root DIR         走査の起点 (default: Source)\n"
		"  --out FILE         チャンクをJSONで書き出す\n"
		"  --exclude SUBSTR   パスに含まれたら除外（複数指定可）\n"
		"  --no-default-exclude  既定の除外(Backends)を無効化\n"
		"  --top N            最長チャンクをN件表示 (default: 10)\n"
		"  --quiet            統計のみ出力\n"
		"\n"
		"  --db FILE          SQLite索引を構築/差分更新する\n"
		"  --dim N            Mock埋め込みの次元 (default: 256)\n"
		"  --query TEXT       --db の索引を検索する（構築せず検索のみ）\n"
		"  --topk N           検索結果の表示件数 (default: 8)\n"
		"\n"
		"注: 埋め込みは MockEmbeddingBackend（語彙ベース）を使う。\n"
		"    実モデルは Service/LlamaEmbeddingBackend の接続後に差し替える。\n");
}

std::int64_t NowMillis() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ---------------------------------
// SQLite索引の構築（差分更新）
// ---------------------------------
int RunIndexBuild(const CodeIndexOptions& options, const std::string& dbPath, std::size_t dim) {
	MockEmbeddingBackend backend(dim);

	CodeIndexStore store;
	Result r = store.Open(dbPath);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }
	r = store.EnsureSchema();
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	// 埋め込みモデルが変わったらベクトルは全て無効。
	// 古いベクトルを黙って使い続ける事故を防ぐため、索引に記録して照合する。
	std::string recordedModel;
	r = store.GetMeta("embedding_model", &recordedModel);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	const bool modelChanged = (!recordedModel.empty() && recordedModel != backend.Name());
	if(modelChanged) {
		std::printf("埋め込みモデルが変わった (%s -> %s)。全件を再構築する。\n",
		            recordedModel.c_str(), backend.Name().c_str());
	}
	r = store.SetMeta("embedding_model", backend.Name());
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	std::vector<std::string> paths;
	int skipped = 0;
	r = EnumerateSourceFiles(options, &paths, &skipped);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	const std::int64_t startMs = NowMillis();

	int reindexed = 0;
	int unchanged = 0;
	int failed = 0;
	std::size_t totalChunks = 0;

	for(const std::string& path : paths) {
		std::string source;
		if(!ReadSourceFile(path, &source)) { ++failed; continue; }

		const std::string hash = CodeIndexStore::HashContent(source);

		std::string storedHash;
		r = store.GetFileHash(path, &storedHash);
		if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

		// 差分更新の核。内容が変わっていないファイルは触らない。
		// 日常的にはここでほぼ全件がスキップされ、数ファイルだけが再埋め込みされる。
		if(!modelChanged && !storedHash.empty() && storedHash == hash) {
			++unchanged;
			continue;
		}

		std::vector<CodeChunk> chunks = ParseSourceFile(path, source, nullptr);

		std::vector<std::string> texts;
		texts.reserve(chunks.size());
		for(const CodeChunk& c : chunks) texts.push_back(c.EmbedText());

		std::vector<std::vector<float>> vectors;
		r = backend.Embed(texts, &vectors);
		if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

		r = store.ReplaceFile(path, hash, chunks, vectors);
		if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

		++reindexed;
		totalChunks += chunks.size();
	}

	int removed = 0;
	r = store.RemoveFilesNotIn(paths, &removed);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	const std::int64_t elapsed = NowMillis() - startMs;

	std::int64_t stored = 0;
	r = store.CountChunks(&stored);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	std::printf("==== index build (%s) ====\n", dbPath.c_str());
	std::printf("model            : %s (dim=%zu)\n", backend.Name().c_str(), dim);
	std::printf("files            : %zu (skipped %d, failed %d)\n", paths.size(), skipped, failed);
	std::printf("  reindexed      : %d  (%zu chunks embedded)\n", reindexed, totalChunks);
	std::printf("  unchanged      : %d  <- 差分更新でスキップ\n", unchanged);
	std::printf("  stale removed  : %d\n", removed);
	std::printf("chunks in index  : %lld\n", static_cast<long long>(stored));
	std::printf("elapsed          : %lld ms\n", static_cast<long long>(elapsed));
	return 0;
}

// ---------------------------------
// 検索
// ---------------------------------
int RunSearch(const std::string& dbPath, const std::string& query,
              std::size_t dim, std::size_t topK) {
	MockEmbeddingBackend backend(dim);

	CodeIndexStore store;
	Result r = store.Open(dbPath);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }

	std::int64_t loadStart = NowMillis();
	std::vector<StoredChunk> chunks;
	r = store.LoadAll(&chunks);
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }
	const std::int64_t loadMs = NowMillis() - loadStart;

	SearchIndex index;
	const std::int64_t buildStart = NowMillis();
	r = index.Build(std::move(chunks));
	if(!r) { std::fprintf(stderr, "ERROR: %s\n", r.error.c_str()); return 1; }
	const std::int64_t buildMs = NowMillis() - buildStart;

	std::printf("index: %zu chunks (%zu embedded, dim=%zu) load=%lldms build=%lldms\n",
	            index.Size(), index.EmbeddedCount(), index.Dimensions(),
	            static_cast<long long>(loadMs), static_cast<long long>(buildMs));
	std::printf("query: %s\n", query.c_str());

	const std::vector<float> qv = backend.EmbedOne(query);

	const auto show = [&](const char* label, const std::vector<SearchHit>& hits, long long ms) {
		std::printf("\n---- %s (%lld us) ----\n", label, ms);
		for(std::size_t i = 0; i < hits.size(); ++i) {
			const StoredChunk& sc = index.At(hits[i].index);
			std::printf("%2zu. %-52s %s:%d\n",
			            i + 1, sc.chunk.qualifiedName.c_str(),
			            sc.chunk.filePath.c_str(), sc.chunk.startLine);
		}
	};

	using namespace std::chrono;
	auto t0 = steady_clock::now();
	const std::vector<SearchHit> lex = index.SearchLexical(query, topK);
	auto t1 = steady_clock::now();
	const std::vector<SearchHit> vec = index.SearchVector(qv, topK);
	auto t2 = steady_clock::now();
	const std::vector<SearchHit> hyb = index.SearchHybrid(query, qv, topK);
	auto t3 = steady_clock::now();

	show("lexical", lex, duration_cast<microseconds>(t1 - t0).count());
	show("vector", vec, duration_cast<microseconds>(t2 - t1).count());

	std::printf("\n---- hybrid RRF (%lld us) ----\n",
	            static_cast<long long>(duration_cast<microseconds>(t3 - t2).count()));
	for(std::size_t i = 0; i < hyb.size(); ++i) {
		const StoredChunk& sc = index.At(hyb[i].index);
		std::printf("%2zu. %-52s [L%d V%d] %s:%d\n",
		            i + 1, sc.chunk.qualifiedName.c_str(),
		            hyb[i].lexicalRank, hyb[i].vectorRank,
		            sc.chunk.filePath.c_str(), sc.chunk.startLine);
	}
	return 0;
}

// パーセンタイルを取る（昇順ソート済みの配列に対して）。
std::size_t Percentile(const std::vector<std::size_t>& sorted, double p) {
	if(sorted.empty()) return 0;
	std::size_t idx = static_cast<std::size_t>(static_cast<double>(sorted.size()) * p);
	if(idx >= sorted.size()) idx = sorted.size() - 1;
	return sorted[idx];
}

} // namespace

int main(int argc, char** argv) {
	CodeIndexOptions options;
	std::string outPath;
	std::string dbPath;
	std::string query;
	int topCount = 10;
	int topK = 8;
	int dim = 256;
	bool quiet = false;
	bool clearedDefaults = false;

	for(int i = 1; i < argc; ++i) {
		const char* a = argv[i];
		const auto next = [&](const char* name) -> const char* {
			if(i + 1 >= argc) {
				std::fprintf(stderr, "%s には値が必要\n", name);
				return nullptr;
			}
			return argv[++i];
		};

		if(std::strcmp(a, "--root") == 0) {
			const char* v = next("--root");
			if(!v) return 1;
			options.root = v;
		} else if(std::strcmp(a, "--out") == 0) {
			const char* v = next("--out");
			if(!v) return 1;
			outPath = v;
		} else if(std::strcmp(a, "--exclude") == 0) {
			const char* v = next("--exclude");
			if(!v) return 1;
			if(!clearedDefaults) {
				// 明示指定があったら既定値は置き換える方が意図に沿う。
				options.excludeSubstrings.clear();
				clearedDefaults = true;
			}
			options.excludeSubstrings.emplace_back(v);
		} else if(std::strcmp(a, "--no-default-exclude") == 0) {
			options.excludeSubstrings.clear();
			clearedDefaults = true;
		} else if(std::strcmp(a, "--top") == 0) {
			const char* v = next("--top");
			if(!v) return 1;
			topCount = std::atoi(v);
		} else if(std::strcmp(a, "--db") == 0) {
			const char* v = next("--db");
			if(!v) return 1;
			dbPath = v;
		} else if(std::strcmp(a, "--query") == 0) {
			const char* v = next("--query");
			if(!v) return 1;
			query = v;
		} else if(std::strcmp(a, "--dim") == 0) {
			const char* v = next("--dim");
			if(!v) return 1;
			dim = std::atoi(v);
		} else if(std::strcmp(a, "--topk") == 0) {
			const char* v = next("--topk");
			if(!v) return 1;
			topK = std::atoi(v);
		} else if(std::strcmp(a, "--quiet") == 0) {
			quiet = true;
		} else if(std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
			PrintUsage();
			return 0;
		} else {
			std::fprintf(stderr, "不明な引数: %s\n", a);
			PrintUsage();
			return 1;
		}
	}

	const std::size_t dimensions = (dim > 0) ? static_cast<std::size_t>(dim) : 256;

	// --- 検索モード ---
	if(!query.empty()) {
		if(dbPath.empty()) {
			std::fprintf(stderr, "--query には --db が必要\n");
			return 1;
		}
		return RunSearch(dbPath, query, dimensions,
		                 (topK > 0) ? static_cast<std::size_t>(topK) : 8);
	}

	// --- SQLite索引の構築モード ---
	if(!dbPath.empty()) {
		return RunIndexBuild(options, dbPath, dimensions);
	}

	// --- 解析のみ（統計とJSON出力） ---
	std::vector<CodeChunk> chunks;
	CodeIndexReport report;

	const Result result = BuildCodeIndex(options, &chunks, &report);
	if(!result) {
		std::fprintf(stderr, "ERROR: %s\n", result.error.c_str());
		return 1;
	}

	// ---- トークン長分布 ----
	std::vector<std::size_t> tokens;
	std::vector<std::size_t> fnTokens;
	std::vector<std::size_t> typeTokens;
	tokens.reserve(chunks.size());
	for(const CodeChunk& c : chunks) {
		const std::size_t t = c.EstimatedTokens();
		tokens.push_back(t);
		if(c.kind == CodeChunkKind::Function) fnTokens.push_back(t);
		else typeTokens.push_back(t);
	}
	std::sort(tokens.begin(), tokens.end());
	std::sort(fnTokens.begin(), fnTokens.end());
	std::sort(typeTokens.begin(), typeTokens.end());

	std::printf("==== CodeIndex ====\n");
	std::printf("root                 : %s\n", options.root.c_str());
	std::printf("files scanned        : %d (skipped %d, failed %d)\n",
	            report.filesScanned, report.filesSkipped, report.filesFailed);
	std::printf("function chunks      : %d\n", report.stats.functionCount);
	std::printf("type chunks          : %d\n", report.stats.typeCount);
	std::printf("declarations skipped : %d\n", report.stats.forwardDeclarationCount);
	std::printf("total chunks         : %zu\n", report.totalChunks);
	std::printf("total est. tokens    : %zu\n", report.totalEstimatedTokens);

	const auto dump = [](const char* label, const std::vector<std::size_t>& v) {
		if(v.empty()) {
			std::printf("%-10s : (none)\n", label);
			return;
		}
		std::printf("%-10s : n=%-5zu median=%-5zu p90=%-5zu p95=%-5zu p99=%-6zu max=%zu\n",
		            label, v.size(),
		            Percentile(v, 0.50), Percentile(v, 0.90),
		            Percentile(v, 0.95), Percentile(v, 0.99), v.back());
	};
	std::printf("\n---- token distribution ----\n");
	dump("function", fnTokens);
	dump("type", typeTokens);
	dump("all", tokens);

	std::printf("\n---- context limit fit ----\n");
	const double total = static_cast<double>(report.totalChunks ? report.totalChunks : 1);
	std::printf("over 2048 tok (EmbeddingGemma-300M) : %zu (%.1f%%)\n",
	            report.over2048Tokens,
	            static_cast<double>(report.over2048Tokens) / total * 100.0);
	std::printf("over 8192 tok                       : %zu (%.1f%%)\n",
	            report.over8192Tokens,
	            static_cast<double>(report.over8192Tokens) / total * 100.0);

	if(!quiet && topCount > 0) {
		std::vector<const CodeChunk*> byLength;
		byLength.reserve(chunks.size());
		for(const CodeChunk& c : chunks) byLength.push_back(&c);
		std::sort(byLength.begin(), byLength.end(),
		          [](const CodeChunk* a, const CodeChunk* b) {
			          return a->EstimatedTokens() > b->EstimatedTokens();
		          });

		std::printf("\n---- longest chunks (top %d) ----\n", topCount);
		const int limit = std::min(topCount, static_cast<int>(byLength.size()));
		for(int i = 0; i < limit; ++i) {
			const CodeChunk* c = byLength[static_cast<std::size_t>(i)];
			std::printf("%6zu tok  %-8s  %s:%d-%d  %s\n",
			            c->EstimatedTokens(), ToString(c->kind),
			            c->filePath.c_str(), c->startLine, c->endLine,
			            c->qualifiedName.c_str());
		}
	}

	if(!outPath.empty()) {
		std::ofstream out(outPath);
		if(!out) {
			std::fprintf(stderr, "ERROR: 出力を開けない: %s\n", outPath.c_str());
			return 1;
		}
		Json doc;
		doc["report"] = report.ToJson();
		doc["chunks"] = ChunksToJson(chunks);
		out << doc.dump(2) << '\n';
		std::printf("\nwrote %s\n", outPath.c_str());
	}

	return 0;
}
