// =======================================================================
//
// CapabilitySet.h
//
// Agent/Workerごとの許可Toolリスト＋権限レベルを管理する（構想§5 CapabilitySet）。
// トークンはプロセス内ランダム値の秘密文字列で照合する。
//
// =======================================================================
#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../AgentOsTypes.h"
#include "CommandTypes.h" // CapabilityToken

namespace agentos {

// ---------------------------------
// CapabilityRegistry
// スレッドセーフ。IssueToken/Validate/Revokeいずれも内部mutexで保護する。
// ---------------------------------
class CapabilityRegistry {
public:
	// agentに対しallowedTools（"*"は全Tool許可）とmaxPermissionを束ねたトークンを発行する。
	CapabilityToken IssueToken(
		const AgentId& agent,
		std::vector<ToolName> allowedTools,
		PermissionLevel maxPermission);

	// tokenがtoolをrequiredLevelで実行してよいかを検証する。
	// 失敗時はフィールドを特定できるメッセージを返す。
	Result Validate(const CapabilityToken& token, const ToolName& tool, PermissionLevel requiredLevel) const;

	// tokenに紐づくGrantを失効させる。
	void Revoke(const CapabilityToken& token);

private:
	struct Grant {
		AgentId owner;
		std::vector<ToolName> allowedTools;
		PermissionLevel maxPermission = PermissionLevel::Read;
	};

	mutable std::mutex mutex_;
	std::unordered_map<std::string, Grant> grants_; // key: token.secret
};

} // namespace agentos
