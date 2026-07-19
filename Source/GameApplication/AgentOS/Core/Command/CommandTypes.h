// =======================================================================
//
// CommandTypes.h
//
// LLM出力はすべて「提案」であり、CommandRequestとして検証パイプラインを
// 通過したものだけが実行される（構想§3.1）。
//
// =======================================================================
#pragma once

#include <string>

#include "../AgentOsTypes.h"
#include "../Json.h"

namespace agentos {

// ---------------------------------
// Capability Token
// CapabilitySetが発行し、CommandPipelineが照合する。
// ---------------------------------
struct CapabilityToken {
	AgentId owner;                // 発行先Agent
	std::string secret;           // 照合用（プロセス内ランダム値）
};

// ---------------------------------
// Command要求
// ---------------------------------
struct CommandRequest {
	CommandId id = kInvalidId;    // 0ならPipelineが採番
	TaskId taskId = kInvalidId;   // 監査用の所属Task
	AgentId issuer;
	ToolName tool;
	Json arguments;               // Toolごとのスキーマで検証される
	CapabilityToken capability;
	bool dryRun = false;          // trueならPreconditionまでで停止
};

// ---------------------------------
// Command結果
// ---------------------------------
struct CommandResult {
	CommandStatus status = CommandStatus::Ok;
	Json payload;                 // Tool実行結果（Evidence素材）
	std::string error;

	bool IsOk() const noexcept { return status == CommandStatus::Ok; }

	static CommandResult Ok(Json payload = Json::object()) {
		CommandResult r;
		r.payload = std::move(payload);
		return r;
	}
	static CommandResult Fail(CommandStatus status, std::string error) {
		CommandResult r;
		r.status = status;
		r.error = std::move(error);
		return r;
	}
};

// ---------------------------------
// Tool記述子
// argumentSchemaは簡易JSONスキーマ（CommandSchema.hが解釈する）。
// ---------------------------------
struct ToolDescriptor {
	ToolName name;
	std::string description;
	PermissionLevel requiredPermission = PermissionLevel::Read;
	Json argumentSchema;          // {"field": {"type":"integer","required":true,...}, ...}
};

// ---------------------------------
// Tool実体インターフェース
// エンジン側（EngineTools）とテスト用Fakeがこれを実装する。
// PreconditionとExecuteを分離することでDry Runを実現する。
// ---------------------------------
class ICommandExecutor {
public:
	virtual ~ICommandExecutor() = default;

	virtual const ToolDescriptor& Descriptor() const = 0;

	// 実行前条件の検査。状態を変更してはならない。
	virtual Result CheckPrecondition(const Json& arguments) = 0;

	// 実行本体。
	virtual CommandResult Execute(const Json& arguments) = 0;
};

} // namespace agentos
