#pragma once
#include <cstdio>
#include <windows.h>

namespace SE {

enum class LogLevel { Debug, Info, Warning, Error, Fatal };

class Logger
{
public:
    static Logger& Get();

    void Initialize(const char* logFilePath = nullptr);
    void Shutdown();
    void Log(LogLevel level, const char* file, int line, const char* fmt, ...);

private:
    Logger() = default;

    FILE* m_logFile    = nullptr;
    bool  m_hasConsole = false;
};

} // namespace SE

// Strips full path down to just the filename for readable log lines.
#define SE_FILENAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define SE_LOG(level, fmt, ...)  ::SE::Logger::Get().Log(level, SE_FILENAME, __LINE__, fmt, ##__VA_ARGS__)
#define SE_LOG_DEBUG(fmt, ...)   SE_LOG(::SE::LogLevel::Debug,   fmt, ##__VA_ARGS__)
#define SE_LOG_INFO(fmt, ...)    SE_LOG(::SE::LogLevel::Info,    fmt, ##__VA_ARGS__)
#define SE_LOG_WARN(fmt, ...)    SE_LOG(::SE::LogLevel::Warning, fmt, ##__VA_ARGS__)
#define SE_LOG_ERROR(fmt, ...)   SE_LOG(::SE::LogLevel::Error,   fmt, ##__VA_ARGS__)
#define SE_LOG_FATAL(fmt, ...)   SE_LOG(::SE::LogLevel::Fatal,   fmt, ##__VA_ARGS__)

// Checks an HRESULT; logs and breaks on failure. No-op in Release.
#ifdef SE_DEBUG
#define SE_HR(expr) \
    do { \
        HRESULT _hr = (expr); \
        if (FAILED(_hr)) { \
            SE_LOG_ERROR("HRESULT 0x%08X  ->  " #expr, _hr); \
            __debugbreak(); \
        } \
    } while (0)
#else
#define SE_HR(expr) (void)(expr)
#endif
