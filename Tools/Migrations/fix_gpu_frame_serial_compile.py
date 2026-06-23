from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def patch(path: str, old: str, new: str, label: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8-sig")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


patch(
    "Source/GameApplication/Service/Graphics/graphicsContext.cpp",
    "\tm_GpuTimingAvailable = false;\n\tm_GpuFrameTimeValid = false;\n\tm_ActiveGpuTimingIndex = -1;",
    "\tm_GpuTimingAvailable = false;\n\tm_ResolvedGpuFrameTimings.clear();\n\tm_ActiveGpuTimingIndex = -1;",
    "GraphicsContext shutdown legacy timing state",
)

patch(
    "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp",
    "#include <Psapi.h>\n#include <algorithm>\n",
    "#include <Psapi.h>\n#include <algorithm>\n#include <cstddef>\n",
    "PerformanceMonitor size include",
)

patch(
    "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp",
    "template<typename T, size_t N>\nvoid ShiftSamples(T (&samples)[N]){\n\tfor(size_t index = 0; index + 1 < N; ++index){\n\t\tsamples[index] = samples[index + 1];\n\t}\n}\n",
    "template<typename T, std::size_t N>\nvoid ShiftSamples(T (&samples)[N]){\n\tfor(std::size_t index = 0; index + 1 < N; ++index){\n\t\tsamples[index] = samples[index + 1];\n\t}\n}\n\n"
    "template<typename T, std::size_t N>\n"
    "void ShiftSamples(std::array<T, N>& samples){\n"
    "\tfor(std::size_t index = 0; index + 1 < N; ++index){\n"
    "\t\tsamples[index] = samples[index + 1];\n"
    "\t}\n"
    "}\n",
    "PerformanceMonitor std::array sample shift",
)

patch(
    "Source/GameApplication/Engine/Editor/UI/PerformanceMonitor.cpp",
    "\tfor(size_t index = 0; index < SAMPLE_LENGTH; ++index){",
    "\tfor(std::size_t index = 0; index < SAMPLE_LENGTH; ++index){",
    "PerformanceMonitor serial sample index",
)

print("Frame Serial compile fixes applied.")
