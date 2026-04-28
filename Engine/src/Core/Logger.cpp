#include "Engine/Core/Logger.h"
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <share.h>

namespace SE {

static const char* k_LevelTag[] = { "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL" };

// Console colours: dark grey, white, yellow, red, bright red.
static const WORD k_LevelColour[] = { 8, 15, 14, 12, 12 };

Logger& Logger::Get()
{
    static Logger s_instance;
    return s_instance;
}

void Logger::Initialize(const char* logFilePath)
{
#ifdef SE_DEBUG
    if (AllocConsole())
    {
        freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
        freopen_s(reinterpret_cast<FILE**>(stderr), "CONOUT$", "w", stderr);
        m_hasConsole = true;

        // Give the console window a sensible title.
        SetConsoleTitleA("FoxEngine — Debug Console");

        // Widen the console buffer so long lines don't wrap awkwardly.
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi))
        {
            csbi.dwSize.X = 200;
            SetConsoleScreenBufferSize(hOut, csbi.dwSize);
        }
    }
#endif

    if (logFilePath)
        m_logFile = _fsopen(logFilePath, "w", _SH_DENYNO);

    SE_LOG_INFO("Logger initialised");
}

void Logger::Shutdown()
{
    SE_LOG_INFO("Logger shutting down");

    if (m_logFile)
    {
        fclose(m_logFile);
        m_logFile = nullptr;
    }

#ifdef SE_DEBUG
    if (m_hasConsole)
    {
        FreeConsole();
        m_hasConsole = false;
    }
#endif
}

void Logger::Log(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    // Format the user message.
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Build the full log line.
    char line_buf[1200];
    snprintf(line_buf, sizeof(line_buf), "[%s] %s(%d): %s\n",
             k_LevelTag[static_cast<int>(level)], file, line, msg);

    // VS Output window.
    OutputDebugStringA(line_buf);

    // Console with colour.
    if (m_hasConsole)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hOut, k_LevelColour[static_cast<int>(level)]);
        fputs(line_buf, stdout);
        // Reset to white.
        SetConsoleTextAttribute(hOut, 15);
    }

    // Log file (no colour codes).
    if (m_logFile)
    {
        fputs(line_buf, m_logFile);
        fflush(m_logFile);
    }
}

} // namespace SE
