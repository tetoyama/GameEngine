from pathlib import Path


def load(path: str) -> str:
    return Path(path).read_text(encoding="utf-8-sig")


def save(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


# -----------------------------------------------------------------------------
# GraphicsContext public contract and state.
# -----------------------------------------------------------------------------
path = "Source/GameApplication/Service/Graphics/graphicsContext.h"
text = load(path)
text = replace_once(text, "#include <array>\n", "#include <array>\n#include <cstdint>\n", "graphics cstdint include")
text = replace_once(
    text,
    """\tvoid Clear(const float clearColor[4]);
\tvoid Present(bool vsync);
""",
    """\tvoid Clear(const float clearColor[4]);
\tvoid WaitForFrameLatency();
\tvoid Present(bool vsync);
\tbool IsFrameLatencyWaitableObjectEnabled() const noexcept {
\t\treturn m_FrameLatencyWaitableObjectEnabled;
\t}
\tuint64_t GetFrameLatencyWaitTimeoutCount() const noexcept {
\t\treturn m_FrameLatencyWaitTimeoutCount;
\t}
""",
    "graphics pacing public methods",
)
text = replace_once(
    text,
    """\tRenderEffectSystem* m_EffectSystem = nullptr;
\tbool m_TearingSupported = false;
};
""",
    """\tRenderEffectSystem* m_EffectSystem = nullptr;
\tbool m_TearingSupported = false;

\t// Flip-model SwapChain pacing. The actual creation flags are retained so
\t// ResizeBuffers always receives exactly the same flag set.
\tUINT m_SwapChainFlags = 0;
\tHANDLE m_FrameLatencyWaitableObject = nullptr;
\tbool m_FrameLatencyWaitableObjectEnabled = false;
\tbool m_FrameLatencyWaitWarningLogged = false;
\tuint64_t m_FrameLatencyWaitTimeoutCount = 0;
\tstatic constexpr DWORD kFrameLatencyWaitTimeoutMilliseconds = 100;
};
""",
    "graphics pacing members",
)
save(path, text)

# -----------------------------------------------------------------------------
# GraphicsContext implementation.
# -----------------------------------------------------------------------------
path = "Source/GameApplication/Service/Graphics/graphicsContext.cpp"
text = load(path)
text = replace_once(
    text,
    """\tm_d2dFactory.Reset();
\tm_dwriteFactory.Reset();

\tm_DeviceContext.Reset();
\tm_SwapChain.Reset();
""",
    """\tm_d2dFactory.Reset();
\tm_dwriteFactory.Reset();

\tif(m_FrameLatencyWaitableObject){
\t\tCloseHandle(m_FrameLatencyWaitableObject);
\t\tm_FrameLatencyWaitableObject = nullptr;
\t}
\tm_FrameLatencyWaitableObjectEnabled = false;
\tm_FrameLatencyWaitTimeoutCount = 0;

\tm_DeviceContext.Reset();
\tm_SwapChain.Reset();
""",
    "close frame latency handle",
)
text = replace_once(
    text,
    """\t// ティアリング対応環境では、DXGI_SWAP_EFFECT_FLIP_DISCARD と DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING を使用する。
\tdesc.SwapEffect = m_TearingSupported ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
\tdesc.Flags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
""",
    """\t// ティアリング対応環境ではFlip Modelを使用し、Frame Latency Waitable Objectで
\t// CPUの先行投入量を1フレームへ制限する。これによりQueueが満杯になった時の
\t// 不定期なPresentブロックをFrame開始前の明示的な待機へ移す。
\tdesc.SwapEffect = m_TearingSupported ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
\tm_SwapChainFlags = m_TearingSupported
\t\t? (DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
\t\t: 0;
\tdesc.Flags = m_SwapChainFlags;
""",
    "swapchain pacing flags",
)
old_create = """\thr = D3D11CreateDeviceAndSwapChain(
\t\tpSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
\t\tfeatureLevels, _countof(featureLevels),
\t\tD3D11_SDK_VERSION, &desc, m_SwapChain.GetAddressOf(),
\t\tm_Device.GetAddressOf(), &featureLevel, m_DeviceContext.GetAddressOf()
\t);

\tif(FAILED(hr)){
\t\treleaseFactoryResources();
\t\tGRAPHICS_LOG(LogLevel::Error, "スワップチェーンの作成に失敗しました");
\t\treturn false;
\t}

\treleaseFactoryResources();
"""
new_create = """\thr = D3D11CreateDeviceAndSwapChain(
\t\tpSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
\t\tfeatureLevels, _countof(featureLevels),
\t\tD3D11_SDK_VERSION, &desc, m_SwapChain.GetAddressOf(),
\t\tm_Device.GetAddressOf(), &featureLevel, m_DeviceContext.GetAddressOf()
\t);

\t// 古いDXGI RuntimeやDriverではWaitable Object flagを拒否する場合がある。
\t// その場合はALLOW_TEARINGを維持したまま、Waitable flagだけを外して再試行する。
\tif(FAILED(hr) && (m_SwapChainFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0){
\t\tm_SwapChain.Reset();
\t\tm_DeviceContext.Reset();
\t\tm_Device.Reset();
\t\tm_SwapChainFlags &= ~DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
\t\tdesc.Flags = m_SwapChainFlags;
\t\thr = D3D11CreateDeviceAndSwapChain(
\t\t\tpSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
\t\t\tfeatureLevels, _countof(featureLevels),
\t\t\tD3D11_SDK_VERSION, &desc, m_SwapChain.GetAddressOf(),
\t\t\tm_Device.GetAddressOf(), &featureLevel, m_DeviceContext.GetAddressOf()
\t\t);
\t\tif(SUCCEEDED(hr)){
\t\t\tGRAPHICS_LOG(LogLevel::Warning, "Frame Latency Waitable Object非対応のため通常SwapChainへフォールバックしました");
\t\t}
\t}

\tif(FAILED(hr)){
\t\treleaseFactoryResources();
\t\tGRAPHICS_LOG(LogLevel::Error, "スワップチェーンの作成に失敗しました");
\t\treturn false;
\t}

\tif((m_SwapChainFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0){
\t\tMicrosoft::WRL::ComPtr<IDXGISwapChain2> swapChain2;
\t\tif(SUCCEEDED(m_SwapChain.As(&swapChain2)) &&
\t\t   SUCCEEDED(swapChain2->SetMaximumFrameLatency(1))){
\t\t\tm_FrameLatencyWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
\t\t\tm_FrameLatencyWaitableObjectEnabled = m_FrameLatencyWaitableObject != nullptr;
\t\t}
\t}

\t// Waitable Objectが使えない環境でもDXGI Device側のキュー長を1へ制限する。
\tif(!m_FrameLatencyWaitableObjectEnabled){
\t\tMicrosoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice1;
\t\tif(SUCCEEDED(m_Device.As(&dxgiDevice1))){
\t\t\tdxgiDevice1->SetMaximumFrameLatency(1);
\t\t}
\t}

\treleaseFactoryResources();
"""
text = replace_once(text, old_create, new_create, "swapchain creation and pacing setup")
text = replace_once(
    text,
    """void GraphicsContext::Present(bool vsync){
""",
    """void GraphicsContext::WaitForFrameLatency(){
\tif(!m_FrameLatencyWaitableObjectEnabled || !m_FrameLatencyWaitableObject){
\t\treturn;
\t}

\tconst DWORD waitResult = WaitForSingleObject(
\t\tm_FrameLatencyWaitableObject,
\t\tkFrameLatencyWaitTimeoutMilliseconds
\t);
\tif(waitResult == WAIT_OBJECT_0){
\t\treturn;
\t}

\tif(waitResult == WAIT_TIMEOUT){
\t\t++m_FrameLatencyWaitTimeoutCount;
\t\tif(!m_FrameLatencyWaitWarningLogged){
\t\t\tm_FrameLatencyWaitWarningLogged = true;
\t\t\tGRAPHICS_LOG(LogLevel::Warning, "Frame Latency Waitが100msでTimeoutしました。Device/GPU stallを確認してください");
\t\t}
\t\treturn;
\t}

\tif(!m_FrameLatencyWaitWarningLogged){
\t\tm_FrameLatencyWaitWarningLogged = true;
\t\tGRAPHICS_LOG(LogLevel::Warning, "Frame Latency Waitable Objectの待機に失敗しました");
\t}
}

void GraphicsContext::Present(bool vsync){
""",
    "frame latency wait function",
)
text = replace_once(
    text,
    """\t\tUINT resizeFlags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
\t\tHRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resizeFlags);
""",
    """\t\t// 作成時に実際に使用したflagを完全に維持する。
\t\tHRESULT hr = m_SwapChain->ResizeBuffers(
\t\t\t0,
\t\t\twidth,
\t\t\theight,
\t\t\tDXGI_FORMAT_UNKNOWN,
\t\t\tm_SwapChainFlags
\t\t);
""",
    "resize uses actual swapchain flags",
)
save(path, text)

# -----------------------------------------------------------------------------
# TimeService draw breakdown.
# -----------------------------------------------------------------------------
path = "Source/GameApplication/Service/Runtime/TimeService/timeService.h"
text = load(path)
text = replace_once(
    text,
    """enum class DrawTimingSection : unsigned char {
\tFrameSetup,
""",
    """enum class DrawTimingSection : unsigned char {
\tFramePacingWait,
\tFrameSetup,
""",
    "draw pacing enum",
)
text = replace_once(
    text,
    """struct DrawTimingBreakdown {
\tdouble frameSetup = 0.0;
""",
    """struct DrawTimingBreakdown {
\tdouble framePacingWait = 0.0;
\tdouble frameSetup = 0.0;
""",
    "draw pacing field",
)
text = replace_once(
    text,
    """\t\treturn frameSetup +
""",
    """\t\treturn framePacingWait +
\t\t\tframeSetup +
""",
    "draw pacing accounted",
)
save(path, text)

path = "Source/GameApplication/Service/Runtime/TimeService/timeService.cpp"
text = load(path)
text = replace_once(
    text,
    """\tswitch(section){
\t\tcase DrawTimingSection::FrameSetup:
""",
    """\tswitch(section){
\t\tcase DrawTimingSection::FramePacingWait:
\t\t\tcurrentDrawTiming_.framePacingWait += elapsedSeconds;
\t\t\tbreak;
\t\tcase DrawTimingSection::FrameSetup:
""",
    "draw pacing accumulation",
)
save(path, text)

# -----------------------------------------------------------------------------
# Engine execution point and editor snapshot.
# -----------------------------------------------------------------------------
path = "Source/GameApplication/Engine/engine.cpp"
text = load(path)
text = replace_once(
    text,
    """\t\ttime->BeginDraw();
\t\tgraphics->BeginGpuFrameTiming();

\t\ttime->BeginDrawSection(DrawTimingSection::FrameSetup);
""",
    """\t\ttime->BeginDraw();

\t\t// SwapChain queueの空きをFrame開始前に待つ。
\t\t// Present内で不定期にまとめて待たされる状態を避け、独立区間として計測する。
\t\ttime->BeginDrawSection(DrawTimingSection::FramePacingWait);
\t\tgraphics->WaitForFrameLatency();
\t\ttime->EndDrawSection(DrawTimingSection::FramePacingWait);

\t\tgraphics->BeginGpuFrameTiming();

\t\ttime->BeginDrawSection(DrawTimingSection::FrameSetup);
""",
    "engine pacing wait",
)
text = replace_once(
    text,
    """\t\t\tdraw.TearingSupported = graphics->IsTearingSupported();
\t\t\tdraw.ResizeSerial = renderer->GetResizeSerial();
""",
    """\t\t\tdraw.TearingSupported = graphics->IsTearingSupported();
\t\t\tdraw.FrameLatencyWaitableObjectEnabled = graphics->IsFrameLatencyWaitableObjectEnabled();
\t\t\tdraw.FrameLatencyWaitTimeoutCount = graphics->GetFrameLatencyWaitTimeoutCount();
\t\t\tdraw.ResizeSerial = renderer->GetResizeSerial();
""",
    "editor pacing snapshot",
)
save(path, text)

path = "Source/GameApplication/Engine/Editor/editorService.h"
text = load(path)
text = replace_once(
    text,
    """\tbool VSyncEnabled = false;
\tbool TearingSupported = false;

\t// MainRendererのResize実績。serialが変化したフレームをResize直後として扱う。
""",
    """\tbool VSyncEnabled = false;
\tbool TearingSupported = false;
\tbool FrameLatencyWaitableObjectEnabled = false;
\tuint64_t FrameLatencyWaitTimeoutCount = 0;

\t// MainRendererのResize実績。serialが変化したフレームをResize直後として扱う。
""",
    "editor pacing fields",
)
save(path, text)

# -----------------------------------------------------------------------------
# Performance Monitor samples and spike output.
# -----------------------------------------------------------------------------
path = "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.h"
text = load(path)
text = replace_once(
    text,
    """\t\tfloat gpuMilliseconds = 0.0f;
\t\tfloat renderMilliseconds = 0.0f;
""",
    """\t\tfloat gpuMilliseconds = 0.0f;
\t\tfloat framePacingMilliseconds = 0.0f;
\t\tfloat renderMilliseconds = 0.0f;
""",
    "spike pacing field",
)
text = replace_once(
    text,
    """\tfloat FrameSetupSamples[SAMPLE_LENGTH]{};
""",
    """\tfloat FramePacingWaitSamples[SAMPLE_LENGTH]{};
\tfloat FrameSetupSamples[SAMPLE_LENGTH]{};
""",
    "pacing sample array",
)
save(path, text)

path = "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp"
text = load(path)
text = replace_once(
    text,
    """\tconst float gpuMs = ctx.GPUFrameTimeValid
\t\t? static_cast<float>(ctx.GPUFrameTime * 1000.0)
\t\t: 0.0f;
\tconst float renderMs = static_cast<float>(ctx.DrawTiming.renderSchedule * 1000.0);
""",
    """\tconst float gpuMs = ctx.GPUFrameTimeValid
\t\t? static_cast<float>(ctx.GPUFrameTime * 1000.0)
\t\t: 0.0f;
\tconst float framePacingMs = static_cast<float>(ctx.DrawTiming.framePacingWait * 1000.0);
\tconst float renderMs = static_cast<float>(ctx.DrawTiming.renderSchedule * 1000.0);
""",
    "record pacing milliseconds",
)
text = replace_once(
    text,
    """\trecord.gpuMilliseconds = gpuMs;
\trecord.renderMilliseconds = renderMs;
""",
    """\trecord.gpuMilliseconds = gpuMs;
\trecord.framePacingMilliseconds = framePacingMs;
\trecord.renderMilliseconds = renderMs;
""",
    "store pacing milliseconds",
)
text = replace_once(
    text,
    """\tconsiderDominant("Update CPU", updateMs);
\tconsiderDominant("Render Schedule CPU", renderMs);
""",
    """\tconsiderDominant("Update CPU", updateMs);
\tconsiderDominant("Frame Pacing Wait", framePacingMs);
\tconsiderDominant("Render Schedule CPU", renderMs);
""",
    "dominant pacing section",
)
text = replace_once(
    text,
    """\tpushMilliseconds(UpdateSamples, Update);
\tpushMilliseconds(DrawSamples, Draw);
\tpushMilliseconds(FrameSetupSamples, ctx.DrawTiming.frameSetup);
""",
    """\tpushMilliseconds(UpdateSamples, Update);
\tpushMilliseconds(DrawSamples, Draw);
\tpushMilliseconds(FramePacingWaitSamples, ctx.DrawTiming.framePacingWait);
\tpushMilliseconds(FrameSetupSamples, ctx.DrawTiming.frameSetup);
""",
    "push pacing samples",
)
text = replace_once(
    text,
    """\t\tImGui::Text("VSync: %s", ctx.VSyncEnabled ? "ON" : "OFF");
\t\tImGui::Text("Tearing: %s", ctx.TearingSupported ? "Supported" : "Unsupported");
""",
    """\t\tImGui::Text("VSync: %s", ctx.VSyncEnabled ? "ON" : "OFF");
\t\tImGui::Text("Tearing: %s", ctx.TearingSupported ? "Supported" : "Unsupported");
\t\tImGui::Text(
\t\t\t"Frame Pacing: %s / Timeouts %llu",
\t\t\tctx.FrameLatencyWaitableObjectEnabled ? "Waitable Object" : "DXGI Fallback",
\t\t\tstatic_cast<unsigned long long>(ctx.FrameLatencyWaitTimeoutCount)
\t\t);
""",
    "display pacing status",
)
text = replace_once(
    text,
    """\t\tdrawTimingRow("Frame Setup", "##DrawFrameSetup", FrameSetupSamples);
""",
    """\t\tdrawTimingRow("Frame Pacing Wait", "##FramePacingWait", FramePacingWaitSamples);
\t\tdrawTimingRow("Frame Setup", "##DrawFrameSetup", FrameSetupSamples);
""",
    "display pacing row",
)
text = replace_once(
    text,
    """\t\tdrawTimingRow("Present / Queue Wait", "##DrawPresent", PresentSamples);
""",
    """\t\tdrawTimingRow("Present / Residual Queue Wait", "##DrawPresent", PresentSamples);
""",
    "rename present row",
)
text = replace_once(
    text,
    """\t\t\t\tImGui::TextDisabled(
\t\t\t\t\t"Update %.3f / Draw %.3f / GPU %.3f / Render %.3f / Editor %.3f / Present %.3f ms",
\t\t\t\t\tspike.updateMilliseconds,
\t\t\t\t\tspike.drawMilliseconds,
\t\t\t\t\tspike.gpuMilliseconds,
\t\t\t\t\tspike.renderMilliseconds,
\t\t\t\t\tspike.editorMilliseconds,
\t\t\t\t\tspike.presentMilliseconds
\t\t\t\t);
""",
    """\t\t\t\tImGui::TextDisabled(
\t\t\t\t\t"Update %.3f / Draw %.3f / GPU %.3f / Pacing %.3f / Render %.3f / Editor %.3f / Present %.3f ms",
\t\t\t\t\tspike.updateMilliseconds,
\t\t\t\t\tspike.drawMilliseconds,
\t\t\t\t\tspike.gpuMilliseconds,
\t\t\t\t\tspike.framePacingMilliseconds,
\t\t\t\t\tspike.renderMilliseconds,
\t\t\t\t\tspike.editorMilliseconds,
\t\t\t\t\tspike.presentMilliseconds
\t\t\t\t);
""",
    "spike pacing details",
)
save(path, text)
