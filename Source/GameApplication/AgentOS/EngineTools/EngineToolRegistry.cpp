// =======================================================================
//
// EngineToolRegistry.cpp
//
// =======================================================================
#include "EngineToolRegistry.h"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "EntityIntrospection.h"
#include "SystemIntrospection.h"

#include "Scene/scene.h"
#include "Scene/Entity/Entity.h"
#include "Scene/Registry/componentRegistry.h"

namespace agentos {

namespace {

// ---------------------------------
// LambdaTool
// ICommandExecutorをstd::functionで組み立てる汎用アダプタ。
// Preconditionは省略時 常にOk（このファイルのTool群はすべて読み取り専用/観測系で、
// 状態を変更する事前条件チェックが不要なため）。
// ---------------------------------
class LambdaTool : public ICommandExecutor {
public:
	using ExecuteFn = std::function<CommandResult(const Json&)>;
	using PreconditionFn = std::function<Result(const Json&)>;

	LambdaTool(ToolDescriptor descriptor, ExecuteFn execute, PreconditionFn precondition = nullptr)
		: m_descriptor(std::move(descriptor))
		, m_execute(std::move(execute))
		, m_precondition(std::move(precondition)) {}

	const ToolDescriptor& Descriptor() const override { return m_descriptor; }

	Result CheckPrecondition(const Json& arguments) override {
		if(m_precondition) return m_precondition(arguments);
		return Result::Ok();
	}

	CommandResult Execute(const Json& arguments) override {
		if(!m_execute){
			return CommandResult::Fail(CommandStatus::ExecutionFailed, "tool not implemented");
		}
		return m_execute(arguments);
	}

private:
	ToolDescriptor m_descriptor;
	ExecuteFn m_execute;
	PreconditionFn m_precondition;
};

// ---------------------------------
// Schema構築ヘルパー
// ---------------------------------
Json Schema(std::initializer_list<std::pair<const char*, Json>> fields) {
	Json schema = Json::object();
	for(const auto& [name, spec] : fields){
		schema[name] = spec;
	}
	return schema;
}

Json StringField(bool required) {
	Json field = Json::object();
	field["type"] = "string";
	field["required"] = required;
	return field;
}

Json IntegerField(bool required, std::optional<int> minValue, std::optional<int> maxValue) {
	Json field = Json::object();
	field["type"] = "integer";
	field["required"] = required;
	if(minValue) field["min"] = *minValue;
	if(maxValue) field["max"] = *maxValue;
	return field;
}

// payloadが {"infrastructure": true} を持つ場合はTool自体が実行不能（Scene未ロード等）
// だったことを意味し、CommandStatus::ExecutionFailedとして扱う。
// それ以外（Entity未検出等のビジネスロジック上の空振り）はOkのまま返す
// （LLM側が結果を見て次の判断をできるようにするため）。
CommandResult FinishOrFail(Json payload) {
	if(payload.is_object() && payload.value("infrastructure", false)){
		return CommandResult::Fail(
			CommandStatus::ExecutionFailed,
			payload.value("error", std::string("engine tool execution failed"))
		);
	}
	return CommandResult::Ok(std::move(payload));
}

Json MakeInfrastructureError(const std::string& message) {
	Json error = Json::object();
	error["error"] = message;
	error["infrastructure"] = true;
	return error;
}

} // namespace

void RegisterEngineTools(
	CommandPipeline& pipeline,
	EngineToolContext& engineContext,
	MainThreadDispatcher& dispatcher,
	WriteTracer& tracer
) {
	// -----------------------------------------------------------------
	// ListSystems
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"ListSystems",
			"登録済みSystemTaskとDomainごとの依存関係を一覧する。",
			PermissionLevel::Read,
			Json::object()
		},
		[&engineContext, &dispatcher](const Json&) -> CommandResult {
			Json payload = dispatcher.RunOnMainThread([&engineContext]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");
				return ExportSystemDescriptors(*sceneContext);
			});

			if(!payload.value("infrastructure", false)){
				const std::size_t taskCount = payload.value("taskCount", static_cast<std::size_t>(0));
				payload["claim"] = "登録済みSystemTaskは" + std::to_string(taskCount) + "件。";
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// FindWriters / FindReaders
	// -----------------------------------------------------------------
	auto makeAccessFilterTool = [&engineContext, &dispatcher](const char* name, const char* description, bool writers) {
		return std::make_shared<LambdaTool>(
			ToolDescriptor{
				name,
				description,
				PermissionLevel::Read,
				Schema({{"component", StringField(true)}})
			},
			[&engineContext, &dispatcher, writers](const Json& args) -> CommandResult {
				const std::string component = args.at("component").get<std::string>();

				Json payload = dispatcher.RunOnMainThread([&engineContext]() -> Json {
					SceneContext* sceneContext = engineContext.ResolveSceneContext();
					if(!sceneContext) return MakeInfrastructureError("no active scene");
					return ExportSystemDescriptors(*sceneContext);
				});

				if(payload.value("infrastructure", false)){
					return FinishOrFail(std::move(payload));
				}

				const char* key = writers ? "componentWrites" : "componentReads";
				Json matches = Json::array();
				for(const Json& task : payload.value("tasks", Json::array())){
					bool matched = false;
					for(const Json& componentName : task.value(key, Json::array())){
						if(componentName.is_string() && componentName.get<std::string>() == component){
							matched = true;
							break;
						}
					}
					if(matched) matches.push_back(task);
				}

				Json result = Json::object();
				result["component"] = component;
				result["tasks"] = matches;
				result["claim"] = "Component'" + component + "'を"
					+ (writers ? std::string("書き込む") : std::string("読み取る"))
					+ "SystemTaskは" + std::to_string(matches.size()) + "件。";
				return CommandResult::Ok(result);
			}
		);
	};

	pipeline.RegisterTool(makeAccessFilterTool(
		"FindWriters", "指定Componentへ書き込むSystemTaskを列挙する。", true));
	pipeline.RegisterTool(makeAccessFilterTool(
		"FindReaders", "指定Componentを読み取るSystemTaskを列挙する。", false));

	// -----------------------------------------------------------------
	// ListEntities
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"ListEntities",
			"アクティブSceneの生存Entityを一覧する。",
			PermissionLevel::Read,
			Schema({{"maxCount", IntegerField(false, 1, 1000)}})
		},
		[&engineContext, &dispatcher](const Json& args) -> CommandResult {
			const std::size_t maxCount = args.contains("maxCount")
				? static_cast<std::size_t>(args.at("maxCount").get<int>())
				: static_cast<std::size_t>(100);

			Json payload = dispatcher.RunOnMainThread([&engineContext, maxCount]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				Json result = Json::object();
				result["entities"] = ListEntities(*sceneContext, maxCount);
				return result;
			});

			if(!payload.value("infrastructure", false)){
				payload["claim"] = "Entityを" + std::to_string(payload.value("entities", Json::array()).size()) + "件取得した。";
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// FindEntityByName
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"FindEntityByName",
			"NameComponent.nameが一致するEntityを検索する。",
			PermissionLevel::Read,
			Schema({{"name", StringField(true)}})
		},
		[&engineContext, &dispatcher](const Json& args) -> CommandResult {
			const std::string name = args.at("name").get<std::string>();

			Json payload = dispatcher.RunOnMainThread([&engineContext, name]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				std::optional<Entity> found = FindEntityByName(*sceneContext, name);
				Json result = Json::object();
				result["found"] = found.has_value();
				if(found){
					result["id"] = found->GetIndex();
					result["generation"] = found->GetGeneration();
				}
				return result;
			});

			if(!payload.value("infrastructure", false)){
				const bool found = payload.value("found", false);
				payload["claim"] = found
					? ("Entity '" + name + "' が見つかった。")
					: ("Entity '" + name + "' は見つからなかった。");
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// DescribeEntity
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"DescribeEntity",
			"名前で指定したEntityが持つ全Componentの値を返す。",
			PermissionLevel::Read,
			Schema({{"entityName", StringField(true)}})
		},
		[&engineContext, &dispatcher](const Json& args) -> CommandResult {
			const std::string entityName = args.at("entityName").get<std::string>();

			Json payload = dispatcher.RunOnMainThread([&engineContext, entityName]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				std::optional<Entity> found = FindEntityByName(*sceneContext, entityName);
				if(!found){
					Json result = Json::object();
					result["error"] = "entity not found: " + entityName;
					return result;
				}
				return DescribeEntity(*sceneContext, *found);
			});

			if(!payload.value("infrastructure", false)){
				if(payload.contains("error")){
					payload["claim"] = "Entity '" + entityName + "' の詳細取得に失敗した: "
						+ payload.value("error", std::string());
				} else {
					const std::size_t componentCount = payload.value("components", Json::array()).size();
					payload["claim"] = "Entity '" + entityName + "' のComponentを"
						+ std::to_string(componentCount) + "件取得した。";
				}
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// ReadComponent
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"ReadComponent",
			"名前で指定したEntityの単一Componentの値を読み取る。",
			PermissionLevel::Read,
			Schema({{"entityName", StringField(true)}, {"component", StringField(true)}})
		},
		[&engineContext, &dispatcher](const Json& args) -> CommandResult {
			const std::string entityName = args.at("entityName").get<std::string>();
			const std::string component = args.at("component").get<std::string>();

			Json payload = dispatcher.RunOnMainThread([&engineContext, entityName, component]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				std::optional<Entity> found = FindEntityByName(*sceneContext, entityName);
				if(!found){
					Json result = Json::object();
					result["error"] = "entity not found: " + entityName;
					return result;
				}
				return ReadComponent(*sceneContext, *found, component);
			});

			if(!payload.value("infrastructure", false)){
				if(payload.contains("error")){
					payload["claim"] = "Component読み取りに失敗した: " + payload.value("error", std::string());
				} else {
					payload["claim"] = "Entity '" + entityName + "' のComponent '" + component + "' を読み取った。";
				}
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// StartWriteTrace
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"StartWriteTrace",
			"指定Entity/Componentのフレーム間差分トレースを開始する。",
			PermissionLevel::Observe,
			Schema({{"entityName", StringField(true)}, {"component", StringField(true)}})
		},
		[&engineContext, &dispatcher, &tracer](const Json& args) -> CommandResult {
			const std::string entityName = args.at("entityName").get<std::string>();
			const std::string component = args.at("component").get<std::string>();

			Json payload = dispatcher.RunOnMainThread([&engineContext, &tracer, entityName, component]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				std::optional<Entity> found = FindEntityByName(*sceneContext, entityName);
				if(!found){
					Json result = Json::object();
					result["error"] = "entity not found: " + entityName;
					return result;
				}

				if(sceneContext->component &&
					sceneContext->component->GetComponentIDByName(component) == INVALID_COMPONENT_TYPE_ID){
					Json result = Json::object();
					result["error"] = "unknown component: " + component;
					return result;
				}

				tracer.Clear();
				tracer.SetTarget(*found, component, sceneContext);

				Json result = Json::object();
				result["started"] = true;
				result["entityName"] = entityName;
				result["component"] = component;
				return result;
			});

			if(!payload.value("infrastructure", false)){
				payload["claim"] = payload.value("started", false)
					? ("Entity '" + entityName + "' のComponent '" + component + "' のWriteTraceを開始した。")
					: ("WriteTraceの開始に失敗した: " + payload.value("error", std::string()));
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// StopWriteTrace
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"StopWriteTrace",
			"現在のWriteTraceサンプリングを停止する（蓄積済みイベントは保持する）。",
			PermissionLevel::Observe,
			Json::object()
		},
		[&dispatcher, &tracer](const Json&) -> CommandResult {
			Json payload = dispatcher.RunOnMainThread([&tracer]() -> Json {
				const bool wasActive = tracer.IsActive();
				tracer.Stop();

				Json result = Json::object();
				result["stopped"] = true;
				result["wasActive"] = wasActive;
				return result;
			});

			if(!payload.value("infrastructure", false)){
				payload["claim"] = "WriteTraceを停止した。";
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// GetWriteTrace
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"GetWriteTrace",
			"蓄積済みのWriteTraceイベントを取得する。",
			PermissionLevel::Read,
			Json::object()
		},
		[&dispatcher, &tracer](const Json&) -> CommandResult {
			Json payload = dispatcher.RunOnMainThread([&tracer]() -> Json {
				Json result = Json::object();
				result["active"] = tracer.IsActive();
				result["events"] = tracer.GetTrace();
				return result;
			});

			if(!payload.value("infrastructure", false)){
				const std::size_t eventCount = payload.value("events", Json::array()).size();
				payload["claim"] = "WriteTraceイベントを" + std::to_string(eventCount) + "件取得した。";
			}
			return FinishOrFail(std::move(payload));
		}
	));

	// -----------------------------------------------------------------
	// ExportSystemDescriptors（Logs/AgentOS/systems.json へも書き出す）
	// -----------------------------------------------------------------
	pipeline.RegisterTool(std::make_shared<LambdaTool>(
		ToolDescriptor{
			"ExportSystemDescriptors",
			"SystemTask一覧をJSON化し、Logs/AgentOS/systems.jsonへ書き出す。",
			PermissionLevel::Read,
			Json::object()
		},
		[&engineContext, &dispatcher](const Json&) -> CommandResult {
			Json payload = dispatcher.RunOnMainThread([&engineContext]() -> Json {
				SceneContext* sceneContext = engineContext.ResolveSceneContext();
				if(!sceneContext) return MakeInfrastructureError("no active scene");

				Json exported = ExportSystemDescriptors(*sceneContext);

				std::error_code errorCode;
				std::filesystem::create_directories("Logs/AgentOS", errorCode);
				bool written = false;
				if(!errorCode){
					std::ofstream output("Logs/AgentOS/systems.json", std::ios::out | std::ios::trunc);
					if(output){
						output << exported.dump(2);
						written = static_cast<bool>(output);
					}
				}
				exported["writtenToDisk"] = written;
				if(errorCode) exported["diskError"] = errorCode.message();
				return exported;
			});

			if(!payload.value("infrastructure", false)){
				const std::size_t taskCount = payload.value("taskCount", static_cast<std::size_t>(0));
				payload["claim"] = "SystemTask一覧(" + std::to_string(taskCount)
					+ "件)をLogs/AgentOS/systems.jsonへ出力した。";
			}
			return FinishOrFail(std::move(payload));
		}
	));
}

} // namespace agentos
