// =======================================================================
//
// EngineToolRegistry.h
//
// EngineTools（SystemIntrospection / EntityIntrospection / WriteTracer）を
// ICommandExecutorとしてCommandPipelineへ登録するファクトリ（構想§11）。
//
// =======================================================================
#pragma once

#include "../Core/Command/CommandPipeline.h"
#include "EngineToolContext.h"
#include "MainThreadDispatcher.h"
#include "WriteTrace.h"

namespace agentos {

// EngineTool群をpipelineへ登録する。
// engineContext / dispatcher / tracerは呼び出し側（AgentOSService）が所有し続け、
// 登録されたTool群が生存期間中ずっと参照へアクセスするため、
// 少なくともpipelineと同じか、それより長く生存させること。
void RegisterEngineTools(
	CommandPipeline& pipeline,
	EngineToolContext& engineContext,
	MainThreadDispatcher& dispatcher,
	WriteTracer& tracer
);

} // namespace agentos
