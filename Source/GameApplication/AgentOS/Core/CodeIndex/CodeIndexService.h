// =======================================================================
//
// CodeIndexService.h
//
// コード索引の構築と検索を、エンジンのフレーム処理を妨げずに提供する層。
//
// 設計の要点：
//
//  1. 走査・パースと埋め込みを分けて考える。
//     実測で走査＋パースは542ファイル97,500行で数秒。重いのは埋め込みだけ。
//     よって embedding が未接続でも索引は成立し、字句検索は即座に使える。
//
//  2. 常に差分更新。ファイル内容のハッシュを索引に持ち、変わったものだけ
//     処理する。日常的にはほぼ全件がスキップされ数ファイルだけが再構築される。
//
//  3. 索引の入れ替えはポインタのすげ替えで行う。
//     構築スレッドが新しいSearchIndexを組み立て終えてから差し替えるため、
//     検索側は構築中も古い索引に対して一貫した結果を返せる。
//     初回構築中は索引が空なので、検索は空を返す（クラッシュしない）。
//
// llama.cpp / D3D11 に依存しないため AgentOS/Core に置いている。
// Tests/AgentOS/Makefile からLinuxでビルド・実行して検証できる。
//
// =======================================================================
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../AgentOsTypes.h"
#include "CodeIndexBuilder.h"
#include "CodeSearch.h"
#include "IEmbeddingBackend.h"

namespace agentos {

// ---------------------------------
// 初期化パラメータ
// ---------------------------------
struct CodeIndexServiceContext {
	// 走査の起点。実行時のカレントディレクトリからの相対で解決される。
	std::string sourceRoot = "Source";

	// 索引DBの置き場所。
	std::string databasePath = "AgentOS_CodeIndex.db";

	// 埋め込みバックエンド。
	// nullptr なら「字句検索のみ」の索引になる（ベクトル列はNULLのまま）。
	// 後から SetEmbeddingBackend() で接続して再構築すればベクトルが埋まる。
	// 所有はしない。呼び出し側がサービスより長く生存させること。
	IEmbeddingBackend* embedding = nullptr;

	// 走査オプション（除外パターンなど）
	CodeIndexOptions options;

	// 起動と同時に構築を始めるか。
	// false の場合はワーカースレッド自体を起こさない。
	// RequestRebuild() が呼ばれた時点で初めてスレッドが立つ。
	// （索引を使わない構成で余計なスレッドを抱えないため。
	//   同時に、スレッド起因の不具合を切り分ける際のスイッチにもなる）
	bool buildOnStart = true;
};

// ---------------------------------
// 検索結果（呼び出し側へ値で返す）
// ---------------------------------
// 内部のSearchIndexは構築のたびに差し替わるため、
// 添字や参照を外へ出すと寿命の問題が起きる。値のコピーで返す。
struct CodeSearchResult {
	std::string qualifiedName;
	std::string filePath;
	std::string moduleTag;
	std::string kind;
	int startLine = 0;
	int endLine = 0;
	std::string text;

	float score = 0.0f;
	int lexicalRank = 0;
	int vectorRank = 0;
};

// ---------------------------------
// 進捗と状態
// ---------------------------------
enum class CodeIndexState {
	Idle,      // 未着手
	Building,  // 構築中
	Ready,     // 利用可能
	Failed,    // 失敗（errorに理由）
};

const char* ToString(CodeIndexState state) noexcept;

struct CodeIndexStatus {
	CodeIndexState state = CodeIndexState::Idle;

	int filesTotal = 0;
	int filesProcessed = 0;
	int filesReindexed = 0;
	int filesUnchanged = 0;

	std::size_t chunkCount = 0;
	std::size_t embeddedCount = 0;

	std::int64_t elapsedMillis = 0;
	std::string error;

	// 進捗率 0.0〜1.0（UIの進捗バー用）
	float Progress() const noexcept {
		if(filesTotal <= 0) return 0.0f;
		return static_cast<float>(filesProcessed) / static_cast<float>(filesTotal);
	}
};

// ---------------------------------
// CodeIndexService
// ---------------------------------
class CodeIndexService {
public:
	CodeIndexService() = default;
	~CodeIndexService();

	CodeIndexService(const CodeIndexService&) = delete;
	CodeIndexService& operator=(const CodeIndexService&) = delete;

	// ワーカースレッドを起こす。buildOnStartなら直ちに構築を要求する。
	Result Initialize(CodeIndexServiceContext context);

	// スレッドを畳む。構築中なら中断を要求して待つ。
	void Shutdown();

	// 構築（差分更新）を要求する。既に構築中なら何もしない。
	// force=true なら全ファイルを無条件に再構築する
	// （パーサを直したときや埋め込みモデルを変えたとき用）。
	void RequestRebuild(bool force = false);

	// 埋め込みバックエンドを後から接続する。
	// 接続してもベクトルは自動では埋まらないので、
	// 続けて RequestRebuild(true) を呼ぶこと。
	void SetEmbeddingBackend(IEmbeddingBackend* backend);

	CodeIndexStatus GetStatus() const;

	bool IsReady() const noexcept {
		return m_state.load(std::memory_order_acquire) == CodeIndexState::Ready;
	}

	// 検索。索引が未完成なら空を返す。
	// 埋め込みが未接続なら字句検索のみ、接続済みならハイブリッドになる。
	// filePathFilter が空でなければ、パスにそれを含むチャンクだけへ絞る。
	std::vector<CodeSearchResult> Search(
		const std::string& query,
		std::size_t topK,
		const std::string& filePathFilter = std::string()) const;

	// シンボル名でピンポイントに引く（ランキングを介さない完全一致寄りの探索）。
	// 名前が分かっているときは検索より確実で、本文を切り詰めずに返せる。
	// 一致の優先順位: 完全一致 > 末尾一致(::name) > 部分一致
	std::vector<CodeSearchResult> FindSymbol(
		const std::string& name,
		const std::string& filePathFilter,
		std::size_t limit) const;

private:
	void WorkerMain();
	void RunBuild(bool force);

	// ワーカースレッドを必要になった時点で起こす。既に起きていれば何もしない。
	void EnsureWorkerStarted();

	// 現在有効な索引を安全に取り出す。
	std::shared_ptr<const SearchIndex> AcquireIndex() const;

	CodeIndexServiceContext m_context;

	// --- ワーカー ---
	std::thread m_worker;
	mutable std::mutex m_jobMutex;
	std::condition_variable m_jobCv;
	bool m_buildRequested = false;
	bool m_forceRequested = false;
	std::atomic<bool> m_running{false};
	std::atomic<bool> m_abort{false};
	std::atomic<bool> m_initialized{false};
	mutable std::mutex m_workerLifecycleMutex;

	// --- 索引本体（読み手と書き手で共有する） ---
	mutable std::mutex m_indexMutex;
	std::shared_ptr<const SearchIndex> m_index;

	// --- 埋め込み（後から差し替わりうる） ---
	mutable std::mutex m_embeddingMutex;
	IEmbeddingBackend* m_embedding = nullptr;

	// --- 状態 ---
	std::atomic<CodeIndexState> m_state{CodeIndexState::Idle};
	mutable std::mutex m_statusMutex;
	CodeIndexStatus m_status;
};

} // namespace agentos
