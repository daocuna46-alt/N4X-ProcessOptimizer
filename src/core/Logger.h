#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <deque>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <Windows.h>

namespace n4x {

enum class LogLevel { Info = 0, Warn = 1, Error = 2 };

struct LogEntry {
    std::chrono::system_clock::time_point ts;
    LogLevel level;
    std::string text;
};

class Logger {
public:
    static Logger& Instance();

    void Init(const std::wstring& filePath);
    void Shutdown();

    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);
    void Log(LogLevel level, const std::string& msg);

    std::deque<LogEntry> Snapshot(size_t maxEntries = 500) const;
    std::wstring FilePath() const;
    void SetMinLevel(LogLevel lvl);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Worker();
    static DWORD WINAPI ThreadProc(LPVOID param);
    std::string FormatLine(const LogEntry& e) const;
    static const char* LevelName(LogLevel l);

    mutable std::mutex mtx_;
    std::deque<LogEntry> buffer_;
    std::deque<LogEntry> pending_;
    std::wstring filePath_;
    std::atomic<bool> running_{false};
    std::atomic<int> minLevel_{0};
    HANDLE thread_ = nullptr;
    HANDLE event_ = nullptr;
};

} // namespace n4x
