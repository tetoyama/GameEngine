#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include "Service/Config/BuildTarget.h"

// One native CMake package process, with output redirected to an editor-readable
// log. No worker service or build-system abstraction is needed.
class PortableBuildJob final {
public:
    ~PortableBuildJob() { if(process) CloseHandle(process); }
    PortableBuildJob() = default;
    PortableBuildJob(const PortableBuildJob&) = delete;
    PortableBuildJob& operator=(const PortableBuildJob&) = delete;
    bool Poll() {
        if(!process) return false;
        DWORD code;
        if(GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) {
            CloseHandle(process); process = nullptr;
            status = code == 0 ? "Package build succeeded." : "Package build failed. See the build log.";
        }
        return process != nullptr;
    }
    void Start(BuildTarget target, const std::string& cmakeExecutable) {
        if(Poll()) return;
        if(target != BuildTarget::Windows) { status = "macOS builds require a Mac or the macOS CI runner."; return; }
        try {
            const auto source = std::filesystem::current_path();
            std::filesystem::path executable(std::u8string_view(reinterpret_cast<const char8_t*>(cmakeExecutable.data()), cmakeExecutable.size()));
            if(executable.wstring().find(L'"') != std::wstring::npos) throw std::runtime_error("Invalid CMake executable path.");
            if(!executable.is_absolute()) {
                std::vector<wchar_t> path(32768);
                const DWORD count = SearchPathW(nullptr, executable.c_str(), L".exe", static_cast<DWORD>(path.size()), path.data(), nullptr);
                if(!count || count >= path.size()) throw std::runtime_error("CMake was not found. Set its executable path.");
                executable = path.data();
            }
            logPath = source / "Logs/Build/Windows.log";
            std::filesystem::create_directories(logPath.parent_path());
            SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE log = CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
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
            CloseHandle(info.hThread); process = info.hProcess;
            status = "Building Windows runtime package...";
        } catch(const std::exception& error) { status = error.what(); }
    }
    const std::string& Status() const noexcept { return status; }
    const std::filesystem::path& LogPath() const noexcept { return logPath; }
private:
    HANDLE process = nullptr;
    std::string status;
    std::filesystem::path logPath;
};
