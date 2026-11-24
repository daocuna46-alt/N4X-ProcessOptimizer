#include "Logger.h"
#include <algorithm>

namespace n4x {

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() { Shutdown(); }

DWORD WINAPI Logger::ThreadProc(LPVOID param) {
    auto* self = static_cast<Logger*>(param);
    self->Worker();
    return 0;
}

void Logger::Init(const std::wstring& filePath) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (running_.load()) return;
    filePath_ = filePath;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(filePath_).parent_path(), ec);
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    running_.store(true);
    thread_ = CreateThread(nullptr, 0, &Logger::ThreadProc, this, 0, nullptr);
}

void Logger::Shutdown() {
    bool wasRunning = running_.exchange(false);
    if (wasRunning && event_) SetEvent(event_);
    if (thread_) {
        WaitForSingleObject(thread_, 2000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (event_) {
        CloseHandle(event_);
        event_ = nullptr;
    }
    // flush remaining
    std::deque<LogEntry> batch;
    std::wstring path;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        batch.swap(pending_);
        path = filePath_;
    }
    if (!batch.empty() && !path.empty()) {
        std::ofstream ofs(std::filesystem::path(path), std::ios::app);
        if (ofs) for (const auto& e : batch) ofs << FormatLine(e) << "\n";
    }
}

void Logger::SetMinLevel(LogLevel lvl) { minLevel_.store(static_cast<int>(lvl)); }

void Logger::Info(const std::string& msg) { Log(LogLevel::Info, msg); }
void Logger::Warn(const std::string& msg) { Log(LogLevel::Warn, msg); }
void Logger::Error(const std::string& msg) { Log(LogLevel::Error, msg); }

void Logger::Log(LogLevel level, const std::string& msg) {
    if (static_cast<int>(level) < minLevel_.load()) return;
    LogEntry e;
    e.ts = std::chrono::system_clock::now();
    e.level = level;
    e.text = msg;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        buffer_.push_back(e);
        if (buffer_.size() > 2000) buffer_.pop_front();
        pending_.push_back(e);
        if (pending_.size() > 4000) pending_.pop_front();
    }
    if (event_) SetEvent(event_);
}

std::deque<LogEntry> Logger::Snapshot(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::deque<LogEntry> out;
    size_t start = buffer_.size() > maxEntries ? buffer_.size() - maxEntries : 0;
    for (size_t i = start; i < buffer_.size(); ++i) out.push_back(buffer_[i]);
    return out;
}

std::wstring Logger::FilePath() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return filePath_;
}

const char* Logger::LevelName(LogLevel l) {
    switch (l) {
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string Logger::FormatLine(const LogEntry& e) const {
    auto t = std::chrono::system_clock::to_time_t(e.ts);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(e.ts.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << " [" << LevelName(e.level) << "] " << e.text;
    return oss.str();
}

void Logger::Worker() {
    while (running_.load()) {
        if (event_) WaitForSingleObject(event_, 500);
        std::deque<LogEntry> batch;
        std::wstring path;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            batch.swap(pending_);
            path = filePath_;
        }
        if (!batch.empty() && !path.empty()) {
            std::ofstream ofs(std::filesystem::path(path), std::ios::app);
            if (ofs) for (const auto& e : batch) ofs << FormatLine(e) << "\n";
        }
    }
    // final drain handled in Shutdown
}

} // namespace n4x
