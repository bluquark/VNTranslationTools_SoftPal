#include "pch.h"
#include "RuntimeConfig.h"
#include "SharedConstants.h"
#include "Logger.h"
#include "external/json.hpp"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

static std::wstring Utf8ToWstring(const std::string& utf8Str)
{
    if (utf8Str.empty())
        return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), nullptr, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), &wstr[0], sizeNeeded);
    return wstr;
}

void RuntimeConfig::Load()
{
    if (_loaded)
        return;

    const char* configFileName = RUNTIME_CONFIG_FILENAME;

    std::ifstream file(configFileName);
    if (!file.is_open())
    {
        std::wstringstream ss;
        ss << L"Configuration file not found: " << Utf8ToWstring(configFileName) << L"\n\n";
        ss << L"Please ensure VNTranslationToolsConstants.json is in the game directory.";
        ShowErrorAndExit(ss.str());
    }

    json config;
    try
    {
        // Parse with comment support (// and /* */ style comments)
        config = json::parse(file, nullptr, true, true);
    }
    catch (const json::parse_error& e)
    {
        std::wstringstream ss;
        ss << L"Failed to parse configuration file: " << Utf8ToWstring(configFileName) << L"\n\n";
        ss << L"Error: " << Utf8ToWstring(e.what());
        ShowErrorAndExit(ss.str());
    }

    try
    {
        _debugLogging = config.value("debugLogging", true);
        _enableFontSubstitution = config.value("enableFontSubstitution", true);
        _customFontFilename = Utf8ToWstring(config.at("customFontFilename").get<std::string>());
        _monospaceFontFilename = Utf8ToWstring(config.at("monospaceFontFilename").get<std::string>());
        _fontHeightIncrease = config.at("fontHeightIncrease").get<int>();
        _fontYSpacingBetweenLines = config.at("fontYSpacingBetweenLines").get<int>();
        _fontYTopPosDecrease = config.at("fontYTopPosDecrease").get<int>();
        _proportionalLineWidth = config.at("proportionalLineWidth").get<int>();
        _maxLineWidth = config.at("maxLineWidth").get<int>();
        _numLinesWarnThreshold = config.at("numLinesWarnThreshold").get<int>();

        // Read graphicsMode string (required, no default)
        if (!config.contains("graphicsMode")) {
            ShowErrorAndExit(L"Missing required setting: graphicsMode\n\n"
                L"Valid values: \"raw\", \"dx9\", \"dx11\"");
        }
        std::string graphicsMode = config.at("graphicsMode").get<std::string>();

        // Convert graphicsMode to boolean flags
        if (graphicsMode == "raw") {
            _pillarboxedFullscreen = false;
            _directX11Upscaling = false;
        } else if (graphicsMode == "dx9") {
            _pillarboxedFullscreen = true;
            _directX11Upscaling = false;
        } else if (graphicsMode == "dx11") {
            _pillarboxedFullscreen = true;
            _directX11Upscaling = true;
        } else {
            ShowErrorAndExit(L"Invalid graphicsMode value: \"" + Utf8ToWstring(graphicsMode) + L"\"\n\n"
                L"Valid values: \"raw\", \"dx9\", \"dx11\"");
        }
    }
    catch (const json::exception& e)
    {
        std::wstringstream ss;
        ss << L"Missing or invalid configuration value in: " << Utf8ToWstring(configFileName) << L"\n\n";
        ss << L"Error: " << Utf8ToWstring(e.what());
        ShowErrorAndExit(ss.str());
    }

    _loaded = true;

    // Debug: Log loaded values to confirm config was read
    proxy_log(LogCategory::INIT, "RuntimeConfig::Load() SUCCESS - Config loaded:");
    proxy_log(LogCategory::INIT, "  debugLogging: %s", _debugLogging ? "true" : "false");
    proxy_log(LogCategory::INIT, "  enableFontSubstitution: %s", _enableFontSubstitution ? "true" : "false");
    proxy_log(LogCategory::INIT, "  graphicsMode: %s (pillarboxed=%s, dx11=%s)",
        _pillarboxedFullscreen ? (_directX11Upscaling ? "dx11" : "dx9") : "raw",
        _pillarboxedFullscreen ? "true" : "false",
        _directX11Upscaling ? "true" : "false");
    proxy_log(LogCategory::INIT, "  customFontFilename: %ls", _customFontFilename.c_str());
    proxy_log(LogCategory::INIT, "  monospaceFontFilename: %ls", _monospaceFontFilename.c_str());
    proxy_log(LogCategory::INIT, "  fontHeightIncrease: %d", _fontHeightIncrease);
    proxy_log(LogCategory::INIT, "  fontYSpacingBetweenLines: %d", _fontYSpacingBetweenLines);
    proxy_log(LogCategory::INIT, "  fontYTopPosDecrease: %d", _fontYTopPosDecrease);
    proxy_log(LogCategory::INIT, "  proportionalLineWidth: %d", _proportionalLineWidth);
    proxy_log(LogCategory::INIT, "  maxLineWidth: %d", _maxLineWidth);
    proxy_log(LogCategory::INIT, "  numLinesWarnThreshold: %d", _numLinesWarnThreshold);
}

bool RuntimeConfig::DebugLogging() { return _debugLogging; }
bool RuntimeConfig::EnableFontSubstitution() { return _enableFontSubstitution; }
bool RuntimeConfig::PillarboxedFullscreen() { return _pillarboxedFullscreen; }
bool RuntimeConfig::DirectX11Upscaling() { return _directX11Upscaling; }
void RuntimeConfig::OverrideToRaw()
{
    if (!_pillarboxedFullscreen)
        return;
    _pillarboxedFullscreen = false;
    _directX11Upscaling = false;
    proxy_log(LogCategory::INIT, "RuntimeConfig: Widescreen game detected - auto-overriding to raw mode");
}
const std::wstring& RuntimeConfig::CustomFontFilename() { return _customFontFilename; }
const std::wstring& RuntimeConfig::MonospaceFontFilename() { return _monospaceFontFilename; }
int RuntimeConfig::FontHeightIncrease() { return _fontHeightIncrease; }
int RuntimeConfig::FontYSpacingBetweenLines() { return _fontYSpacingBetweenLines; }
int RuntimeConfig::FontYTopPosDecrease() { return _fontYTopPosDecrease; }
int RuntimeConfig::ProportionalLineWidth() { return _proportionalLineWidth; }
int RuntimeConfig::MaxLineWidth() { return _maxLineWidth; }
int RuntimeConfig::NumLinesWarnThreshold() { return _numLinesWarnThreshold; }
