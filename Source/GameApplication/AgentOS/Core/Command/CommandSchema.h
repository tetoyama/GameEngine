// =======================================================================
//
// CommandSchema.h
//
// Tool引数の簡易JSONスキーマ検証（構想§5 CommandSchemaRegistry相当）。
// argumentSchemaは {"field": {"type":..., "required":..., ...}, ...} 形式で、
// 型・必須・数値範囲・enum・配列要素型・文字列長を宣言的に検査する。
//
// =======================================================================
#pragma once

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

class SchemaValidator {
public:
	// argumentsをargumentSchemaに照らして検証する。
	// 未知フィールド・必須欠落・型不一致・範囲外・enum違反はすべて失敗として
	// フィールド名を含む分かりやすいエラーメッセージで返す。
	static Result Validate(const Json& arguments, const Json& argumentSchema);
};

} // namespace agentos
