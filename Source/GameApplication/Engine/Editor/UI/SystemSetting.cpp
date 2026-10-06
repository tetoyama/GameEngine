// =======================================================================
//
// SystemSetting.cpp
//
// =======================================================================
#include "SystemSettingImplementation.inl"

bool SystemSetting::PollBuild() {
    if(!m_buildProcess) return false;
    DWORD code;
    if(GetExitCodeProcess(m_buildProcess, &code) && code != STILL_ACTIVE) {
        CloseHandle(m_buildProcess); m_buildProcess = nullptr;
        m_buildStatus = code == 0 ? "Package build succeeded." : "Package build failed. See the build log.";
    }
    return m_buildProcess != nullptr;
}

void SystemSetting::StartBuild(BuildTarget target) {
    if(PollBuild()) return;
    if(target != BuildTarget::Windows) { m_buildStatus = "macOS builds require a Mac or the macOS CI runner."; return; }
    try {
        const auto source = std::filesystem::current_path();
        std::filesystem::path executable(std::u8string_view(reinterpret_cast<const char8_t*>(m_cmakeExecutable.data()), m_cmakeExecutable.size()));
        if(executable.wstring().find(L'"') != std::wstring::npos) throw std::runtime_error("Invalid CMake executable path.");
        if(!executable.is_absolute()) {
            std::vector<wchar_t> path(32768);
            const DWORD count = SearchPathW(nullptr, executable.c_str(), L".exe", static_cast<DWORD>(path.size()), path.data(), nullptr);
            if(!count || count >= path.size()) throw std::runtime_error("CMake was not found. Set its executable path.");
            executable = path.data();
        }
        m_buildLogPath = source / "Logs/Build/Windows.log";
        std::filesystem::create_directories(m_buildLogPath.parent_path());
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE log = CreateFileW(m_buildLogPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &attributes, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(log == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open build log.");
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        std::wstring command = L"\"" + executable.wstring() + L"\" -DTARGET_PLATFORM=Windows \"-DSOURCE_DIR=" +
            source.wstring() + L"\" -DCONFIGURATION=Release -P \"" + (source / "cmake/BuildPortable.cmake").wstring() + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdOutput = startup.hStdError = log; startup.hStdInput = input;
        PROCESS_INFORMATION info{};
        const bool started = input != INVALID_HANDLE_VALUE && CreateProcessW(executable.c_str(), command.data(),
            nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, source.c_str(), &startup, &info);
        CloseHandle(log); if(input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if(!started) throw std::runtime_error("Cannot start CMake package build.");
        CloseHandle(info.hThread); m_buildProcess = info.hProcess;
        m_buildStatus = "Building Windows runtime package...";
    } catch(const std::exception& error) { m_buildStatus = error.what(); }
}
