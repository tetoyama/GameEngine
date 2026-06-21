from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GRAPHICS_CPP = ROOT / "Source/GameApplication/Service/Graphics/graphicsContext.cpp"
ENGINE_CPP = ROOT / "Source/GameApplication/Engine/engine.cpp"
ENGINE_CONTEXT_CPP = ROOT / "Source/GameApplication/Engine/engineContext.cpp"
WORKFLOW = ROOT / ".github/workflows/apply-engine-config-backend.yml"
SCRIPT = Path(__file__).resolve()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def patch_graphics_context() -> None:
    text = GRAPHICS_CPP.read_text(encoding="utf-8")

    text = replace_once(
        text,
        '#include "renderEffectSystem.h"\n#include "DebugTools/DebugSystem.h"\n',
        '#include "renderEffectSystem.h"\n'
        '#include "DebugTools/DebugSystem.h"\n'
        '#include "Config/configSystem.h"\n'
        '#include "RHI/Null/NullRHIBackend.h"\n'
        '#include "RHI/D3D11/D3D11RHIBackend.h"\n',
        "graphics includes",
    )

    text = replace_once(
        text,
        'bool GraphicsContext::Initialize(HWND hwnd, UINT width, UINT height){\n'
        '\tGRAPHICS_LOG(LogLevel::Info, "GraphicsContext の初期化を開始します");\n\n'
        '\tif(!CreateDeviceAndSwapChain(hwnd, width, height)){\n',
        'bool GraphicsContext::Initialize(HWND hwnd, UINT width, UINT height){\n'
        '\treturn Initialize(hwnd, width, height, EngineGraphicsConfig{});\n'
        '}\n\n'
        'bool GraphicsContext::Initialize(\n'
        '\tHWND hwnd,\n'
        '\tUINT width,\n'
        '\tUINT height,\n'
        '\tconst EngineGraphicsConfig& graphicsConfig\n'
        '){\n'
        '\tGRAPHICS_LOG(LogLevel::Info, "GraphicsContext の初期化を開始します");\n\n'
        '\tif(!CreateDeviceAndSwapChain(hwnd, width, height, graphicsConfig)){\n',
        "graphics initialize overload",
    )

    start = text.find(
        'bool GraphicsContext::CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height){'
    )
    end = text.find('bool GraphicsContext::CreateDepthStencilState(){', start)
    if start < 0 or end < 0:
        raise RuntimeError("CreateDeviceAndSwapChain boundaries not found")

    replacement = '''bool GraphicsContext::CreateDeviceAndSwapChain(
\tHWND hwnd,
\tUINT width,
\tUINT height,
\tconst EngineGraphicsConfig& graphicsConfig
){
\t// Registry登録は冪等。既にSmoke Test等で登録済みでも問題にしない。
\tRegisterNullRHIBackend();
\tRegisterD3D11RHIBackend();

\tm_SelectedBackend = graphicsConfig.backend;
\tm_RHIBackend = RHI::BackendRegistry::Instance().Create(m_SelectedBackend);
\tif(!m_RHIBackend){
\t\tGRAPHICS_LOG(
\t\t\tLogLevel::Error,
\t\t\t"EngineConfigで指定されたRHI Backendは未登録です: " +
\t\t\tstd::string(ToEngineConfigBackendName(m_SelectedBackend))
\t\t);
\t\treturn false;
\t}
\tif(!m_RHIBackend->IsSupported()){
\t\tGRAPHICS_LOG(
\t\t\tLogLevel::Error,
\t\t\t"EngineConfigで指定されたRHI Backendは現在の環境で利用できません: " +
\t\t\tstd::string(m_RHIBackend->GetName())
\t\t);
\t\tm_RHIBackend.reset();
\t\treturn false;
\t}

\tRHI::DeviceCreateDesc deviceDesc;
\tdeviceDesc.nativeWindow.window = hwnd;
\tdeviceDesc.swapChain.width = width;
\tdeviceDesc.swapChain.height = height;
\tdeviceDesc.swapChain.bufferCount = graphicsConfig.swapChainBufferCount;
\tdeviceDesc.swapChain.format = RHI::Format::RGBA8_UNorm;
\tdeviceDesc.swapChain.allowTearing = graphicsConfig.allowTearing;
\tdeviceDesc.adapterIndex = graphicsConfig.adapterIndex;
\tdeviceDesc.preferHighPerformanceAdapter =
\t\tgraphicsConfig.preferHighPerformanceAdapter;
#if defined(_DEBUG)
\tdeviceDesc.enableDebugLayer = true;
#endif

\tm_RHIDevice = m_RHIBackend->CreateDevice(deviceDesc);
\tif(!m_RHIDevice){
\t\tGRAPHICS_LOG(
\t\t\tLogLevel::Error,
\t\t\t"RHI Deviceの生成に失敗しました: " +
\t\t\tstd::string(m_RHIBackend->GetName())
\t\t);
\t\tm_RHIBackend.reset();
\t\treturn false;
\t}

\t// Step 17のRenderWorld移行が完了するまではLegacy RendererがD3D11 Native型を使用する。
\t// Config選択自体は共通RHIで行うが、非D3D11 Backendは明示的に拒否し、
\t// Direct3D11へ黙ってFallbackしない。
\tif(m_SelectedBackend != RHI::BackendType::Direct3D11){
\t\tGRAPHICS_LOG(
\t\t\tLogLevel::Error,
\t\t\t"選択されたRHI Backendは登録済みですが、Legacy Rendererとの橋渡しが未実装です: " +
\t\t\tstd::string(m_RHIBackend->GetName())
\t\t);
\t\tm_RHIDevice.reset();
\t\tm_RHIBackend.reset();
\t\treturn false;
\t}

\tauto* d3d11Device = dynamic_cast<RHI::D3D11RHIDevice*>(m_RHIDevice.get());
\tif(!d3d11Device || !d3d11Device->NativeDevice() ||
\t\t!d3d11Device->NativeContext() || !d3d11Device->NativeSwapChain()){
\t\tGRAPHICS_LOG(LogLevel::Error, "D3D11 RHI DeviceのNative Object取得に失敗しました");
\t\tm_RHIDevice.reset();
\t\tm_RHIBackend.reset();
\t\treturn false;
\t}

\tm_Device = d3d11Device->NativeDevice();
\tm_DeviceContext = d3d11Device->NativeContext();
\tm_SwapChain = d3d11Device->NativeSwapChain();
\tm_TearingSupported =
\t\td3d11Device->GetSwapChain()->GetDesc().allowTearing;

\tGRAPHICS_LOG(
\t\tLogLevel::Info,
\t\t"EngineConfigからRHI Backendを選択しました: " +
\t\tstd::string(m_RHIBackend->GetName())
\t);
\treturn true;
}

'''
    text = text[:start] + replacement + text[end:]

    text = replace_once(
        text,
        '\tm_DeviceContext.Reset();\n'
        '\tm_SwapChain.Reset();\n'
        '\tm_Device.Reset();\n'
        '\tGRAPHICS_LOG(LogLevel::Info, "GraphicsContext の終了が完了しました");\n',
        '\tm_DeviceContext.Reset();\n'
        '\tm_SwapChain.Reset();\n'
        '\tm_Device.Reset();\n'
        '\tm_RHIDevice.reset();\n'
        '\tm_RHIBackend.reset();\n'
        '\tGRAPHICS_LOG(LogLevel::Info, "GraphicsContext の終了が完了しました");\n',
        "graphics shutdown RHI ownership",
    )

    present_start = text.find('void GraphicsContext::Present(bool vsync){')
    present_end = text.find('bool GraphicsContext::CreateVertexShader(', present_start)
    if present_start < 0 or present_end < 0:
        raise RuntimeError("Present boundaries not found")
    present = '''void GraphicsContext::Present(bool vsync){
\tif(m_RHIDevice && m_RHIDevice->GetSwapChain()){
\t\tif(!m_RHIDevice->GetSwapChain()->Present(vsync)){
\t\t\tGRAPHICS_LOG(LogLevel::Error, "RHI SwapChainのPresentに失敗しました");
\t\t}
\t\treturn;
\t}

\t// RHI初期化以前の互換経路。
\tconst UINT syncInterval = vsync ? 1u : 0u;
\tconst UINT presentFlags = (!vsync && m_TearingSupported)
\t\t? DXGI_PRESENT_ALLOW_TEARING
\t\t: 0u;
\tif(m_SwapChain){
\t\tm_SwapChain->Present(syncInterval, presentFlags);
\t}
}

'''
    text = text[:present_start] + present + text[present_end:]

    text = replace_once(
        text,
        '\t\tUINT resizeFlags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;\n'
        '\t\tHRESULT hr = m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resizeFlags);\n'
        '\t\tif(FAILED(hr)){\n',
        '\t\tbool resized = false;\n'
        '\t\tif(m_RHIDevice && m_RHIDevice->GetSwapChain()){\n'
        '\t\t\tresized = m_RHIDevice->GetSwapChain()->Resize(width, height);\n'
        '\t\t}\n'
        '\t\telse{\n'
        '\t\t\tconst UINT resizeFlags = m_TearingSupported\n'
        '\t\t\t\t? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING\n'
        '\t\t\t\t: 0u;\n'
        '\t\t\tresized = SUCCEEDED(m_SwapChain->ResizeBuffers(\n'
        '\t\t\t\t0, width, height, DXGI_FORMAT_UNKNOWN, resizeFlags\n'
        '\t\t\t));\n'
        '\t\t}\n'
        '\t\tif(!resized){\n',
        "graphics resize through RHI",
    )

    GRAPHICS_CPP.write_text(text, encoding="utf-8")


def patch_engine() -> None:
    text = ENGINE_CPP.read_text(encoding="utf-8")
    text = replace_once(
        text,
        '    // --- Step 6: DirectX 11 グラフィクスコンテキスト初期化\n'
        '\t//             デバイス・デバイスコンテキスト・スワップチェーンを生成\n'
        '    if (!graphicsContext || \n'
        '        !graphicsContext->Initialize(windowService->GetMainWindow()->GetHWND(), mainWindow->GetWidth(), mainWindow->GetHeight())){\n',
        '    // --- Step 6: EngineConfigで選択されたRHI Backendを初期化\n'
        '\t//             現在のLegacy RendererはD3D11 RHI Deviceへ橋渡しする\n'
        '    if (!graphicsContext ||\n'
        '        !graphicsContext->Initialize(\n'
        '            windowService->GetMainWindow()->GetHWND(),\n'
        '            mainWindow->GetWidth(),\n'
        '            mainWindow->GetHeight(),\n'
        '            configSystem->engineConfig.graphics\n'
        '        )){\n',
        "engine graphics initialization",
    )
    ENGINE_CPP.write_text(text, encoding="utf-8")


def patch_engine_context_comment() -> None:
    text = ENGINE_CONTEXT_CPP.read_text(encoding="utf-8")
    text = replace_once(
        text,
        '\t// GraphicsContext 登録（DirectX 11 デバイス・スワップチェーンの管理）\n',
        '\t// GraphicsContext 登録（EngineConfigで選択されたRHI Device / SwapChainの管理）\n',
        "engine context graphics comment",
    )
    ENGINE_CONTEXT_CPP.write_text(text, encoding="utf-8")


def main() -> None:
    patch_graphics_context()
    patch_engine()
    patch_engine_context_comment()

    # 一回限りのMigration Helperは適用コミットへ残さない。
    if WORKFLOW.exists():
        WORKFLOW.unlink()
    if SCRIPT.exists():
        SCRIPT.unlink()


if __name__ == "__main__":
    main()
