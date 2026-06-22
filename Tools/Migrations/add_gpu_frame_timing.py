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


path = "Source/GameApplication/Service/Graphics/graphicsContext.cpp"
text = load(path)

text = replace_once(
    text,
    """\tif(!CreateFullScreenQuad()){
\t\treturn false;
\t}

\tResize(width, height);
""",
    """\tif(!CreateFullScreenQuad()){
\t\treturn false;
\t}

\tif(!CreateGpuFrameTimingQueries()){
\t\tGRAPHICS_LOG(LogLevel::Warning, "GPU Timestamp Queryの初期化に失敗したためGPU Frame Time計測を無効化します");
\t}

\tResize(width, height);
""",
    "initialize GPU timing queries",
)

text = replace_once(
    text,
    """\tm_d2dFactory.Reset();
\tm_dwriteFactory.Reset();

\tm_DeviceContext.Reset();
""",
    """\tfor(auto& timing : m_GpuTimingQueries){
\t\ttiming.disjoint.Reset();
\t\ttiming.beginTimestamp.Reset();
\t\ttiming.endTimestamp.Reset();
\t\ttiming.pending = false;
\t}
\tm_GpuTimingAvailable = false;
\tm_GpuFrameTimeValid = false;
\tm_ActiveGpuTimingIndex = -1;

\tm_d2dFactory.Reset();
\tm_dwriteFactory.Reset();

\tm_DeviceContext.Reset();
""",
    "shutdown GPU timing queries",
)

present_block = """void GraphicsContext::Present(bool vsync){
\t// 引数を実際に使う。
\t// vsync == true  -> SyncInterval=1 で確実に垂直同期させる。
\t// vsync == false -> ティアリング対応環境のみ ALLOW_TEARING を使って無制限FPSにする。
\t//                   非対応環境では SyncInterval=0, flags=0 にする
\t//                   (Flip Modelではこの場合も実質リフレッシュレート上限になるが、
\t//                    これはAPI/ドライバ側の制約であり、フラグの誤指定による
\t//                    暗黙のフォールバックとは区別する)。
\tUINT syncInterval = vsync ? 1 : 0;
\tUINT presentFlags = 0;
\tif(!vsync && m_TearingSupported){
\t\tpresentFlags = DXGI_PRESENT_ALLOW_TEARING;
\t}

\tm_SwapChain->Present(syncInterval, presentFlags);
}
"""

gpu_methods = present_block + """

bool GraphicsContext::CreateGpuFrameTimingQueries(){
\tif(!m_Device || GetBackendType() != RHI::BackendType::Direct3D11){
\t\treturn false;
\t}

\tD3D11_QUERY_DESC desc{};
\tfor(auto& timing : m_GpuTimingQueries){
\t\tdesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
\t\tif(FAILED(m_Device->CreateQuery(&desc, timing.disjoint.ReleaseAndGetAddressOf()))){
\t\t\treturn false;
\t\t}

\t\tdesc.Query = D3D11_QUERY_TIMESTAMP;
\t\tif(FAILED(m_Device->CreateQuery(&desc, timing.beginTimestamp.ReleaseAndGetAddressOf()))){
\t\t\treturn false;
\t\t}
\t\tif(FAILED(m_Device->CreateQuery(&desc, timing.endTimestamp.ReleaseAndGetAddressOf()))){
\t\t\treturn false;
\t\t}
\t\ttiming.pending = false;
\t}

\tm_GpuTimingWriteIndex = 0;
\tm_ActiveGpuTimingIndex = -1;
\tm_GpuFrameTimeSeconds = 0.0;
\tm_GpuFrameTimeValid = false;
\tm_GpuTimingAvailable = true;
\treturn true;
}

void GraphicsContext::ResolveGpuFrameTimingQueries(){
\tif(!m_GpuTimingAvailable || !m_DeviceContext){
\t\treturn;
\t}

\tfor(auto& timing : m_GpuTimingQueries){
\t\tif(!timing.pending){
\t\t\tcontinue;
\t\t}

\t\tD3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
\t\tconst HRESULT disjointResult = m_DeviceContext->GetData(
\t\t\ttiming.disjoint.Get(),
\t\t\t&disjoint,
\t\t\tsizeof(disjoint),
\t\t\tD3D11_ASYNC_GETDATA_DONOTFLUSH
\t\t);
\t\tif(disjointResult != S_OK){
\t\t\tcontinue;
\t\t}

\t\tUINT64 beginTimestamp = 0;
\t\tUINT64 endTimestamp = 0;
\t\tif(m_DeviceContext->GetData(
\t\t\ttiming.beginTimestamp.Get(),
\t\t\t&beginTimestamp,
\t\t\tsizeof(beginTimestamp),
\t\t\tD3D11_ASYNC_GETDATA_DONOTFLUSH
\t\t) != S_OK){
\t\t\tcontinue;
\t\t}
\t\tif(m_DeviceContext->GetData(
\t\t\ttiming.endTimestamp.Get(),
\t\t\t&endTimestamp,
\t\t\tsizeof(endTimestamp),
\t\t\tD3D11_ASYNC_GETDATA_DONOTFLUSH
\t\t) != S_OK){
\t\t\tcontinue;
\t\t}

\t\ttiming.pending = false;
\t\tif(disjoint.Disjoint || disjoint.Frequency == 0 || endTimestamp < beginTimestamp){
\t\t\tm_GpuFrameTimeValid = false;
\t\t\tcontinue;
\t\t}

\t\tm_GpuFrameTimeSeconds = static_cast<double>(endTimestamp - beginTimestamp) /
\t\t\tstatic_cast<double>(disjoint.Frequency);
\t\tm_GpuFrameTimeValid = true;
\t}
}

void GraphicsContext::BeginGpuFrameTiming(){
\tResolveGpuFrameTimingQueries();
\tif(!m_GpuTimingAvailable || !m_DeviceContext){
\t\treturn;
\t}

\tauto& timing = m_GpuTimingQueries[m_GpuTimingWriteIndex];
\tif(timing.pending){
\t\t// GPUが4フレーム以上遅れている場合でもCPUを待機させず、このフレームは計測をスキップする。
\t\tm_ActiveGpuTimingIndex = -1;
\t\treturn;
\t}

\tm_ActiveGpuTimingIndex = static_cast<int>(m_GpuTimingWriteIndex);
\tm_DeviceContext->Begin(timing.disjoint.Get());
\tm_DeviceContext->End(timing.beginTimestamp.Get());
}

void GraphicsContext::EndGpuFrameTiming(){
\tif(!m_GpuTimingAvailable || !m_DeviceContext || m_ActiveGpuTimingIndex < 0){
\t\treturn;
\t}

\tauto& timing = m_GpuTimingQueries[static_cast<size_t>(m_ActiveGpuTimingIndex)];
\tm_DeviceContext->End(timing.endTimestamp.Get());
\tm_DeviceContext->End(timing.disjoint.Get());
\ttiming.pending = true;

\tm_GpuTimingWriteIndex =
\t\t(m_GpuTimingWriteIndex + 1) % kGpuTimingBufferedFrames;
\tm_ActiveGpuTimingIndex = -1;
}
"""

text = replace_once(text, present_block, gpu_methods, "GPU timing method insertion")
save(path, text)
