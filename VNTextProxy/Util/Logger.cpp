#include "pch.h"
#include "Logger.h"
#include "RuntimeConfig.h"
#include <cstdio>
#include <share.h>
#include <windows.h>

static FILE* g_proxyLog = nullptr;
static LARGE_INTEGER g_qpcFrequency = {};
static LARGE_INTEGER g_qpcStart = {};

static void InitTimestamps()
{
    QueryPerformanceFrequency(&g_qpcFrequency);
    QueryPerformanceCounter(&g_qpcStart);
}

static const char* GetCategoryPrefix(LogCategory category)
{
    switch (category) {
        case LogCategory::SHADER: return "[SHADER] ";
        case LogCategory::DX9:    return "[DX9] ";
        case LogCategory::DX11:   return "[DX11] ";
        case LogCategory::HOOKS:  return "[HOOKS] ";
        case LogCategory::TEXT:   return "[TEXT] ";
        case LogCategory::INIT:  return "[INIT] ";
        case LogCategory::FATAL: return "[FATAL] ";
        default:                  return "[???] ";
    }
}

void proxy_log(LogCategory category, const char* format, ...)
{
    if (!RuntimeConfig::DebugLogging())
        return;

    if (!g_proxyLog)
    {
        InitTimestamps();
        g_proxyLog = _fsopen("./dll_proxy.log", "w", _SH_DENYNO);
    }

    if (g_proxyLog)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsedMs = (double)(now.QuadPart - g_qpcStart.QuadPart) * 1000.0 / (double)g_qpcFrequency.QuadPart;
        int seconds = (int)(elapsedMs / 1000.0);
        double fracMs = elapsedMs - seconds * 1000.0;
        fprintf(g_proxyLog, "[%d.%06.0f] ", seconds, fracMs * 1000.0);

        fputs(GetCategoryPrefix(category), g_proxyLog);

        va_list args;
        va_start(args, format);
        vfprintf(g_proxyLog, format, args);
        va_end(args);

        fprintf(g_proxyLog, "\n");
        fflush(g_proxyLog);
    }
}

void ShowErrorAndExit(const std::wstring& message)
{
    proxy_log(LogCategory::FATAL, "%ls", message.c_str());
    MessageBoxW(nullptr, message.c_str(), L"VNTranslationTools Error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}
