from pathlib import Path

p = Path("Source/GameApplication/Service/Graphics/graphicsContext.cpp")
s = p.read_text(encoding="utf-8-sig")
old = """\t// Waitable Objectが使えない環境でもDXGI Device側のキュー長を1へ制限する。\n\tif(!m_FrameLatencyWaitableObjectEnabled){\n\t\tMicrosoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice1;\n\t\tif(SUCCEEDED(m_Device.As(&dxgiDevice1))){\n\t\t\tdxgiDevice1->SetMaximumFrameLatency(1);\n\t\t}\n\t}\n"""
new = """\tconst bool usesWaitableFlag =\n\t\t(m_SwapChainFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0;\n\n\tif(!usesWaitableFlag){\n\t\tMicrosoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice1;\n\t\tif(SUCCEEDED(m_Device.As(&dxgiDevice1))){\n\t\t\tdxgiDevice1->SetMaximumFrameLatency(1);\n\t\t}\n\t} else if(!m_FrameLatencyWaitableObjectEnabled){\n\t\tGRAPHICS_LOG(LogLevel::Warning, \"Frame latency wait handle is unavailable\");\n\t}\n"""
if s.count(old) != 1:
    raise RuntimeError(s.count(old))
p.write_text(s.replace(old, new, 1), encoding="utf-8", newline="\n")
