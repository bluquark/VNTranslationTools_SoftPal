#pragma once

#include <cstdarg>

enum class LogCategory {
    SHADER,
    DX9,
    DX11,
    HOOKS,
    TEXT
};

void proxy_log(LogCategory category, const char* format, ...);
