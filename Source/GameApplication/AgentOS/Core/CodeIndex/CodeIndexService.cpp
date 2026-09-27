// =======================================================================
//
// CodeIndexService.cpp
//
// =======================================================================
#include "CodeIndexService.h"

#include <chrono>
#include <utility>

#include "CodeIndexStore.h"

namespace agentos {

namespace {

std::int64_t NowMillis() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

const char* KindString(CodeChunkKind kind) { return ToString(kind); }

} // namespace

const char* ToString(CodeIndexState state) noexcept {
	switch(state) {
	case CodeIndexState::Idle:     return "idle";
	case CodeIndexState::Building: return "building";
	case CodeIndexState::Ready:    return "ready";
	case CodeIndexState::Failed:   return "failed";
	}
	return "unknown";
}

CodeIndexService::~CodeIndexService() {
	Shutdown();
}

// =======================================================================
// Initialize
// =======================================================================
Result CodeIndexService::Initialize(CodeIndexServiceContext context) {
	if(m_initialized.load(std::memory_order_acquire)) {
		return Result::Fail("CodeIndexService: 既に初期化済み");
	}

	m_context = std::move(context);
	{
		std::lock_guard<std::mutex> lock(m_embeddingMutex);
		m_embedding = m_context.embedding;
	}

	// 索引を空で用意しておく。構築完了前に検索が来ても
	// nullptr参照にならず、空の結果を返せるようにするため。
	{
		auto empty = std::make_shared<SearchIndex>();
		const Result r = empty->Build({});
		if(!r) return r;
		std::lock_guard<std::mutex> lock(m_indexMutex);
		m_index = std::move(empty);
	}

	m_abort.store(false, std::memory_order_release);
	m_initialized.store(true, std::memory_order_release);

	// スレッドはここでは起こさない。
	// buildOnStart が false のときにワーカーだけ生きている状態を作らないため。
	// 索引を使わない構成でスレッドを抱えずに済み、
	// スレッド起因の不具合を切り分けるスイッチとしても機能する。
	if(m_context.buildOnStart) {
		RequestRebuild(false);
	}
	return Result::Ok();
}

// =======================================================================
// EnsureWorkerStarted
// =======================================================================
void CodeIndexService::EnsureWorkerStarted() {
	std::lock_guard<std::mutex> lock(m_workerLifecycleMutex);
	if(m_running.load(std::memory_order_acquire)) return;
	if(!m_initialized.load(std::memory_order_acquire)) return;

	m_abort.store(false, std::memory_order_release);
	m_running.store(true, std::memory_order_release);
	m_worker = std::thread(&CodeIndexService::WorkerMain, this);
}

// =======================================================================
// Shutdown
// =======================================================================
void CodeIndexService::Shutdown() {
	std::lock_guard<std::mutex> lifecycle(m_workerLifecycleMutex);

	m_initialized.store(false, std::memory_order_release);
	if(!m_running.exchange(false, std::memory_order_acq_rel)) return;

	// 構築中でも速やかに抜けられるよう中断フラグを立ててから起こす。
	m_abort.store(true, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(m_jobMutex);
		m_buildRequested = false;
	}
	m_jobCv.notify_all();

	if(m_worker.joinable()) m_worker.join();
}

// =======================================================================
// RequestRebuild
// =======================================================================
void CodeIndexService::RequestRebuild(bool force) {
	// 最初の要求で初めてワーカーが立ち上がる。
	EnsureWorkerStarted();

	{
		std::lock_guard<std::mutex> lock(m_jobMutex);
		m_buildRequested = true;
		// 強制要求は後から来ても打ち消さない（強い方を採る）
		m_forceRequested = m_forceRequested || force;
	}
	m_jobCv.notify_one();
}

void CodeIndexService::SetEmbeddingBackend(IEmbeddingBackend* backend) {
	std::lock_guard<std::mutex> lock(m_embeddingMutex);
	m_embedding = backend;
}

// =======================================================================
// GetStatus
// =======================================================================
CodeIndexStatus CodeIndexService::GetStatus() const {
	std::lock_guard<std::mutex> lock(m_statusMutex);
	CodeIndexStatus copy = m_status;
	copy.state = m_state.load(std::memory_order_acquire);
	return copy;
}

std::shared_ptr<const SearchIndex> CodeIndexService::AcquireIndex() const {
	std::lock_guard<std::mutex> lock(m_indexMutex);
	return m_index;
}

// =======================================================================
// Search
// =======================================================================
std::vector<CodeSearchResult> CodeIndexService::Search(
	const std::string& query, std::size_t topK, const std::string& filePathFilter) const {

	// shared_ptrをコピーして握るので、この後に索引が差し替わっても
	// 手元のスナップショットは生き続ける。ロックは取得の一瞬だけ。
	const std::shared_ptr<const SearchIndex> index = AcquireIndex();
	if(!index || index->Size() == 0 || topK == 0) return {};

	// ファイル絞り込みがあるときは、絞った後にtopK件残るよう多めに取る。
	const std::size_t fetchCount = filePathFilter.empty() ? topK : (topK * 8);

	std::vector<SearchHit> hits;

	IEmbeddingBackend* embedding = nullptr;
	{
		std::lock_guard<std::mutex> lock(m_embeddingMutex);
		embedding = m_embedding;
	}

	// 埋め込みが使えて、索引にもベクトルが入っているときだけハイブリッド。
	// どちらか欠けていれば字句検索に退避する（初回構築中や未接続時の経路）。
	const bool canVector =
		embedding != nullptr &&
		index->EmbeddedCount() > 0 &&
		embedding->Dimensions() == index->Dimensions();

	if(canVector) {
		std::vector<std::vector<float>> vectors;
		const Result r = embedding->Embed({query}, &vectors);
		if(r && vectors.size() == 1 && vectors[0].size() == index->Dimensions()) {
			hits = index->SearchHybrid(query, vectors[0], fetchCount);
		} else {
			hits = index->SearchLexical(query, fetchCount);
		}
	} else {
		hits = index->SearchLexical(query, fetchCount);
	}

	std::vector<CodeSearchResult> results;
	results.reserve(hits.size());
	for(const SearchHit& hit : hits) {
		const StoredChunk& sc = index->At(hit.index);
		if(!filePathFilter.empty() &&
		   sc.chunk.filePath.find(filePathFilter) == std::string::npos) {
			continue;
		}
		if(results.size() >= topK) break;
		CodeSearchResult r;
		r.qualifiedName = sc.chunk.qualifiedName;
		r.filePath = sc.chunk.filePath;
		r.moduleTag = sc.chunk.moduleTag;
		r.kind = KindString(sc.chunk.kind);
		r.startLine = sc.chunk.startLine;
		r.endLine = sc.chunk.endLine;
		r.text = sc.chunk.text;
		r.score = hit.score;
		r.lexicalRank = hit.lexicalRank;
		r.vectorRank = hit.vectorRank;
		results.push_back(std::move(r));
	}
	return results;
}

// =======================================================================
// FindSymbol
// =======================================================================
std::vector<CodeSearchResult> CodeIndexService::FindSymbol(
	const std::string& name,
	const std::string& filePathFilter,
	std::size_t limit) const {

	const std::shared_ptr<const SearchIndex> index = AcquireIndex();
	if(!index || index->Size() == 0 || name.empty() || limit == 0) return {};

	// 一致の強さで3段に分けて集める。
	// 名前が分かっているクエリでは、ランキングを経由するより
	// 完全一致を直接返す方が確実（埋め込みは原理的にこれを超えられない）。
	std::vector<CodeSearchResult> exact;
	std::vector<CodeSearchResult> suffix;
	std::vector<CodeSearchResult> partial;

	const std::string suffixKey = "::" + name;

	for(std::size_t i = 0; i < index->Size(); ++i) {
		const StoredChunk& sc = index->At(i);
		if(!filePathFilter.empty() &&
		   sc.chunk.filePath.find(filePathFilter) == std::string::npos) {
			continue;
		}

		const std::string& qualified = sc.chunk.qualifiedName;

		std::vector<CodeSearchResult>* bucket = nullptr;
		if(qualified == name) {
			bucket = &exact;
		} else if(qualified.size() > suffixKey.size() &&
		          qualified.compare(qualified.size() - suffixKey.size(),
		                            suffixKey.size(), suffixKey) == 0) {
			bucket = &suffix;
		} else if(qualified.find(name) != std::string::npos) {
			bucket = &partial;
		}
		if(bucket == nullptr) continue;

		CodeSearchResult r;
		r.qualifiedName = qualified;
		r.filePath = sc.chunk.filePath;
		r.moduleTag = sc.chunk.moduleTag;
		r.kind = KindString(sc.chunk.kind);
		r.startLine = sc.chunk.startLine;
		r.endLine = sc.chunk.endLine;
		r.text = sc.chunk.text;
		bucket->push_back(std::move(r));
	}

	std::vector<CodeSearchResult> results;
	for(std::vector<CodeSearchResult>* bucket : {&exact, &suffix, &partial}) {
		for(CodeSearchResult& r : *bucket) {
			if(results.size() >= limit) return results;
			results.push_back(std::move(r));
		}
	}
	return results;
}

// =======================================================================
// WorkerMain
// =======================================================================
void CodeIndexService::WorkerMain() {
	while(true) {
		bool force = false;
		{
			std::unique_lock<std::mutex> lock(m_jobMutex);
			m_jobCv.wait(lock, [this]() {
				return m_buildRequested || !m_running.load(std::memory_order_acquire);
			});
			if(!m_running.load(std::memory_order_acquire)) return;
			m_buildRequested = false;
			force = m_forceRequested;
			m_forceRequested = false;
		}

		RunBuild(force);
	}
}

// =======================================================================
// RunBuild
// =======================================================================
void CodeIndexService::RunBuild(bool force) {
	m_state.store(CodeIndexState::Building, std::memory_order_release);
	const std::int64_t startMs = NowMillis();

	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status = CodeIndexStatus{};
		m_status.state = CodeIndexState::Building;
	}

	const auto fail = [this, startMs](const std::string& message) {
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.error = message;
		m_status.elapsedMillis = NowMillis() - startMs;
		m_state.store(CodeIndexState::Failed, std::memory_order_release);
	};

	CodeIndexStore store;
	Result r = store.Open(m_context.databasePath);
	if(!r) { fail(r.error); return; }
	r = store.EnsureSchema();
	if(!r) { fail(r.error); return; }

	IEmbeddingBackend* embedding = nullptr;
	{
		std::lock_guard<std::mutex> lock(m_embeddingMutex);
		embedding = m_embedding;
	}

	// 索引が別のモデルで作られていたらベクトルは全て無効。
	// 黙って古いベクトルを使い続ける事故を防ぐため、記録して照合する。
	const std::string modelName = embedding ? embedding->Name() : std::string("(none)");
	std::string recordedModel;
	r = store.GetMeta("embedding_model", &recordedModel);
	if(!r) { fail(r.error); return; }
	if(!recordedModel.empty() && recordedModel != modelName) force = true;

	r = store.SetMeta("embedding_model", modelName);
	if(!r) { fail(r.error); return; }

	CodeIndexOptions options = m_context.options;
	options.root = m_context.sourceRoot;

	std::vector<std::string> paths;
	int skipped = 0;
	r = EnumerateSourceFiles(options, &paths, &skipped);
	if(!r) { fail(r.error); return; }

	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.filesTotal = static_cast<int>(paths.size());
	}

	int processed = 0;
	int reindexed = 0;
	int unchanged = 0;

	for(const std::string& path : paths) {
		// Shutdown要求が来ていれば直ちに抜ける。
		// 初回構築は数十分かかりうるため、中断できることが重要。
		if(m_abort.load(std::memory_order_acquire)) {
			fail("中断された");
			return;
		}

		std::string source;
		if(ReadSourceFile(path, &source)) {
			const std::string hash = CodeIndexStore::HashContent(source);

			std::string storedHash;
			r = store.GetFileHash(path, &storedHash);
			if(!r) { fail(r.error); return; }

			if(!force && !storedHash.empty() && storedHash == hash) {
				++unchanged;
			} else {
				std::vector<CodeChunk> chunks = ParseSourceFile(path, source, nullptr);

				std::vector<std::vector<float>> vectors;
				if(embedding != nullptr && !chunks.empty()) {
					std::vector<std::string> texts;
					texts.reserve(chunks.size());
					for(const CodeChunk& c : chunks) texts.push_back(c.EmbedText());
					// 埋め込みに失敗しても索引そのものは作る。
					// ベクトルが無くても字句検索は成立するため、
					// 全体を止めるより価値がある。
					if(!embedding->Embed(texts, &vectors)) vectors.clear();
				}

				r = store.ReplaceFile(path, hash, chunks, vectors);
				if(!r) { fail(r.error); return; }
				++reindexed;
			}
		}

		++processed;
		{
			std::lock_guard<std::mutex> lock(m_statusMutex);
			m_status.filesProcessed = processed;
			m_status.filesReindexed = reindexed;
			m_status.filesUnchanged = unchanged;
		}
	}

	// 索引にあるが今回現れなかったファイル（削除されたソース）を掃除する
	int removed = 0;
	r = store.RemoveFilesNotIn(paths, &removed);
	if(!r) { fail(r.error); return; }

	// --- 新しい索引を組み立ててから差し替える ---
	// 組み立て中も検索側は古い索引を使い続けられる。
	std::vector<StoredChunk> chunks;
	r = store.LoadAll(&chunks);
	if(!r) { fail(r.error); return; }

	auto built = std::make_shared<SearchIndex>();
	r = built->Build(std::move(chunks));
	if(!r) { fail(r.error); return; }

	const std::size_t chunkCount = built->Size();
	const std::size_t embeddedCount = built->EmbeddedCount();

	{
		std::lock_guard<std::mutex> lock(m_indexMutex);
		m_index = std::move(built);
	}

	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.chunkCount = chunkCount;
		m_status.embeddedCount = embeddedCount;
		m_status.elapsedMillis = NowMillis() - startMs;
		m_status.error.clear();
	}
	m_state.store(CodeIndexState::Ready, std::memory_order_release);
}

} // namespace agentos
