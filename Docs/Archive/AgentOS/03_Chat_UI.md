# AgentOS Chat UI

Status: implemented on `llm-agent` (2026-07-17)

## Layout contract

The Chat tab is designed for a docked inspector width of 300–400 px.

1. Compact header: active local model, detected GameEngine/DX11 target, cumulative exact token count,
   API cost, and current generation speed.
2. Timeline: left-aligned user/AgentOS entries without chat bubbles. Markdown fenced code is rendered in
   a dark code surface with lightweight syntax coloring and an adjacent Copy action.
3. Composer: multiline input, `Ctrl+Enter` submission, and a Stop action while a request is active.

## Progressive state contract

- While a session is active, the panel displays observable orchestration stages and the current generated
  output. If a backend emits `<think>...</think>`, that range is separated into the live process surface.
- On completion, live process UI disappears and the final response becomes the primary timeline content.
  The process is retained in a collapsed `Process log (seconds / tokens)` row on that response.
- Process text is an observable model/orchestrator trace. The UI must not manufacture or label hidden model
  state as reasoning when the backend did not emit it.
- Stop latches cancellation for the active session, stops the LLAMA agent, and is reset only by the next
  accepted request.

## Metrics

Prompt and completion token counts come from the LLAMA tokenizer/generation loop. They are not estimated from
character counts. `tk/s` is completion tokens divided by elapsed generation time. `API $0` means that the
selected backend is local and incurs no external API charge; it does not claim zero hardware or electricity cost.

## Apply action

Code blocks show Apply in a disabled state. Enabling it before Phase 5 would bypass the planned audited
`ApplyCodePatch -> Compile -> Test -> Rollback` boundary and would provide neither a target-file contract nor
failure atomicity. Apply becomes interactive only after that pipeline and its human approval gate exist.

## Required Windows verification

- Debug and Release x64 compilation.
- Dock widths of 300, 320, 360, and 400 px.
- Long Japanese response, multiple fenced C++/HLSL blocks, Copy, and `Ctrl+Enter`.
- Active generation -> Stop -> new request.
- Completion transition and reopening the collapsed process log.
- Play -> Stop -> Play and editor shutdown during model load/generation.
