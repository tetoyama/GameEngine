// =======================================================================
//
// RespondTool.h
//
// 会話応答をツールとして提供する。
//
// 以前は「会話か調査か」をIntakeのrequestTypeとキーワード一致で先回りに分類し、
// 会話と判定したら調査パイプラインを丸ごと飛ばしていた。分類が外れるたびに
// キーワードが増える構造で、実際に次の故障を出した。
//   「私は誰ですか？」   → 身元をSceneに探しに行き修復2ラウンド空振りで未完了
//   「私の名前はTaroです」→ 自己紹介が ResolveEntity(Taro) になった
//
// 分類をやめ、会話応答も1つのツールとして一覧に載せる。
// Plannerはツール一覧から普通に選べばよく、完了かどうかは
// その出力を見てCriticが判定する。ツール一覧が正本である。
//
// =======================================================================
#pragma once

#include <functional>

namespace agentos {

class CommandPipeline;
class ILlmBackend;

// Respond を登録する。
//
// バックエンドはポインタではなくプロバイダで受け取ること。
// LLMは初期化後に非同期でロードされるため、初期化時点のポインタを渡すと
// 常にnullptrで、ツールが一度も登録されない。
// 実機ではこれで Respond がツール一覧から消え、Plannerが「こんにちは」に対して
// ListEntities / DescribeEntity / WriteTrace の6タスクを組んで5分走った
// （transcript_20260727_024705）。
void RegisterRespondTool(CommandPipeline& pipeline, std::function<ILlmBackend*()> llmProvider);

// このツールだけで構成されたプランには、Criticの観測要件を適用しない。
// 「観測が要るか」を要求の種類ではなく、実際に使ったツールで決めるための名前。
const char* RespondToolName() noexcept;

} // namespace agentos
