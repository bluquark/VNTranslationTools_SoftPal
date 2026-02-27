#pragma once

#include <cstdarg>
#include <string>

enum class LogCategory {
    SHADER,
    DX9,
    DX11,
    HOOKS,
    TEXT,
    INIT,
    FATAL
};

void proxy_log(LogCategory category, const char* format, ...);

[[noreturn]] void ShowErrorAndExit(const std::wstring& message);
