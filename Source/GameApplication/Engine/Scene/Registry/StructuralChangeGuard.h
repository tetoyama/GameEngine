// =======================================================================
//
// StructuralChangeGuard.h
//
// =======================================================================
#pragma once

#include <atomic>

// Schedule実行中の即時構造変更(AddComponent / RemoveComponent / Storage登録)を
// Debugビルドのassertで検出するためのプロセス全域ガード。
//
// - SystemRegistry::ExecuteTasksがEditor以外のDomain実行中にLockを立てる
// - EntityCommandBuffer::CommitはPlayback区間だけ呼出Thread上でUnlockする
// - Editor Domain / Scene Load / Editor Undo等のSchedule外の即時変更は影響を受けない
//
// (Review H-4 / M-3相当: Docs/ECS_Scheduler_Migration_Plan.md §1.4)
namespace ECSStructural {

inline std::atomic<int>& ScheduleLockDepth() noexcept {
	static std::atomic<int> depth{0};
	return depth;
}

inline int& PlaybackUnlockDepth() noexcept {
	thread_local int depth = 0;
	return depth;
}

// 現在のThreadからの即時構造変更が禁止されているか。
// Schedule実行中(Lock中)かつPlayback区間外のときだけtrueを返す。
inline bool IsImmediateChangeForbidden() noexcept {
	return ScheduleLockDepth().load(std::memory_order_relaxed) > 0 &&
		PlaybackUnlockDepth() == 0;
}

// Schedule実行区間ガード。enabled=falseなら何もしない(Editor Domain用)。
struct ScopedScheduleLock {
	explicit ScopedScheduleLock(bool enabled) noexcept
		: m_enabled(enabled){
		if(m_enabled){
			ScheduleLockDepth().fetch_add(1, std::memory_order_relaxed);
		}
	}
	~ScopedScheduleLock(){
		if(m_enabled){
			ScheduleLockDepth().fetch_sub(1, std::memory_order_relaxed);
		}
	}
	ScopedScheduleLock(const ScopedScheduleLock&) = delete;
	ScopedScheduleLock& operator=(const ScopedScheduleLock&) = delete;

private:
	bool m_enabled = false;
};

// EntityCommandBuffer Playback専用。呼出Thread上でのみ即時適用を許可する。
struct ScopedPlaybackUnlock {
	ScopedPlaybackUnlock() noexcept { ++PlaybackUnlockDepth(); }
	~ScopedPlaybackUnlock(){ --PlaybackUnlockDepth(); }
	ScopedPlaybackUnlock(const ScopedPlaybackUnlock&) = delete;
	ScopedPlaybackUnlock& operator=(const ScopedPlaybackUnlock&) = delete;
};

} // namespace ECSStructural
