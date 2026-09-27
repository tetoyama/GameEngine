// =======================================================================
//
// ConversationHistoryTool.h
//
// 過去セッションの記録を種別横断で引くTool。
//
// 従来、履歴を読むかどうかはIntakeのキーワード一致
// （「続けて」「それ」「さっき」等14語）だけで決まっていた。
// このため「5件全部知りたい」のような明らかな継続が取りこぼされ、
// 会話履歴51件がDBにありながら1件も読まれず、Intakeは情報ゼロで
// turnRelation=clarifyと正しく申告した上で調査に突入して未完了に終わった
// （実機ログ transcript_20260727_013726）。
//
// 先回りで「要るか」を決めるのをやめ、要ると気づいた側
// （Planner / RetrievalWorker / Repair）が自分で取りに行けるようにする。
//
// =======================================================================
#pragma once

namespace agentos {

class CommandPipeline;
class TaskStore;

// GetConversationHistory を登録する。
// storeがnullptrの場合は何も登録しない（履歴を持たない構成のため）。
void RegisterConversationHistoryTool(CommandPipeline& pipeline, TaskStore* store);

} // namespace agentos
