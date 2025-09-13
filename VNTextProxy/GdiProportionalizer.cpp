#include <string>
#include <sstream>
#include <iomanip>

#include "pch.h"

using namespace std;

struct GlyphState {
    double penXF;         // floating pen x
    int    penXI;         // integer pen x as seen by the app (what gmCellIncX drives)
    double carry;         // fractional error accumulator for advances
    double ACarry;        // residual for the left-side A component
    WCHAR  prevCh;        // previous character for kerning (optional)
    // caches: ABCFLOAT for glyphs, kerning table, text metrics…
} state;


std::unordered_map<uint32_t, int> kernAmounts;

/*
Example call sequence:

GdiProportionalizer::CreateFontAHook()
GdiProportionalizer::CreateFontWHook()
GdiProportionalizer::CreateFontIndirectWHook()
GdiProportionalizer::SelectObjectHook()
GdiProportionalizer::GetGlyphOutlineAHook() char: O, 0x4f
GdiProportionalizer::GetGlyphOutlineAHook() char: O, 0x4f
GdiProportionalizer::GetGlyphOutlineAHook() char: O, 0x4f
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: n, 0x6e
GdiProportionalizer::GetGlyphOutlineAHook() char: n, 0x6e
GdiProportionalizer::GetGlyphOutlineAHook() char: n, 0x6e
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: W, 0x57
GdiProportionalizer::GetGlyphOutlineAHook() char: W, 0x57
GdiProportionalizer::GetGlyphOutlineAHook() char: W, 0x57
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: i, 0x69
GdiProportionalizer::GetGlyphOutlineAHook() char: i, 0x69
GdiProportionalizer::GetGlyphOutlineAHook() char: i, 0x69
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char: u, 0x75
GdiProportionalizer::GetGlyphOutlineAHook() char: u, 0x75
GdiProportionalizer::GetGlyphOutlineAHook() char: u, 0x75
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: h, 0x68
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: v, 0x76
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char: o, 0x6f
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char:  , 0x20
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: s, 0x73
GdiProportionalizer::GetGlyphOutlineAHook() char: s, 0x73
GdiProportionalizer::GetGlyphOutlineAHook() char: s, 0x73
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: t, 0x74
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: e, 0x65
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char: r, 0x72
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: d, 0x64
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: a, 0x61
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: y, 0x79
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: ", 0x22
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::SelectObjectHook()
GdiProportionalizer::DeleteObjectHook(
*/

void GdiProportionalizer::Init()
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "wt") == 0 && log) {
        fprintf(log, "GdiProportionalizer::Init() \n");
        fclose(log);
    }
#endif

    Proportionalizer::Init();
    ImportHooker::Hook(
        {
            { "EnumFontsA", EnumFontsAHook },
            { "EnumFontFamiliesExA", EnumFontFamiliesExAHook },
            { "CreateFontA", CreateFontAHook },
            { "CreateFontIndirectA", CreateFontIndirectAHook },
            { "CreateFontW", CreateFontWHook },
            { "CreateFontIndirectW", CreateFontIndirectWHook },
            { "SelectObject", SelectObjectHook },
            { "DeleteObject", DeleteObjectHook },
            { "GetTextExtentPointA", GetTextExtentPointAHook },
            { "GetTextExtentPoint32A", GetTextExtentPoint32AHook },
            { "TextOutA", TextOutAHook },
            { "GetGlyphOutlineA", GetGlyphOutlineAHook }
        }
    );
}

int GdiProportionalizer::EnumFontsAHook(HDC hdc, LPCSTR lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::EnumFontsAHook() \n");
        fclose(log);
    }
#endif

    EnumFontsContext context;
    context.OriginalProc = lpProc;
    context.OriginalContext = lParam;
    context.Extended = false;
    return EnumFontsW(hdc, lpLogfont != nullptr ? SjisTunnelEncoding::Decode(lpLogfont).c_str() : nullptr, &EnumFontsProc, (LPARAM)&context);
}

int GdiProportionalizer::EnumFontFamiliesExAHook(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::EnumFontFamiliesExAHook() \n");
        fclose(log);
    }
#endif

    LOGFONTW logFontW = ConvertLogFontAToW(*lpLogfont);
    EnumFontsContext context;
    context.OriginalProc = lpProc;
    context.OriginalContext = lParam;
    context.Extended = true;
    return EnumFontFamiliesExW(hdc, &logFontW, &EnumFontsProc, (LPARAM)&context, dwFlags);
}

int GdiProportionalizer::EnumFontsProc(const LOGFONTW* lplf, const TEXTMETRICW* lptm, DWORD dwType, LPARAM lpData)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::EnumFontsProc() \n");
        fclose(log);
    }
#endif

    EnumFontsContext* pContext = (EnumFontsContext*)lpData;
    ENUMLOGFONTEXDVA logFontExA;
    logFontExA.elfEnumLogfontEx.elfLogFont = ConvertLogFontWToA(*lplf);
    if (pContext->Extended)
    {
        ENUMLOGFONTEXDVW* pLogFontExW = (ENUMLOGFONTEXDVW*)lplf;
        strcpy_s((char*)logFontExA.elfEnumLogfontEx.elfFullName, sizeof(logFontExA.elfEnumLogfontEx.elfFullName), SjisTunnelEncoding::Encode(pLogFontExW->elfEnumLogfontEx.elfFullName).c_str());
        strcpy_s((char*)logFontExA.elfEnumLogfontEx.elfScript,   sizeof(logFontExA.elfEnumLogfontEx.elfScript),   SjisTunnelEncoding::Encode(pLogFontExW->elfEnumLogfontEx.elfScript).c_str());
        strcpy_s((char*)logFontExA.elfEnumLogfontEx.elfStyle,    sizeof(logFontExA.elfEnumLogfontEx.elfStyle),    SjisTunnelEncoding::Encode(pLogFontExW->elfEnumLogfontEx.elfStyle).c_str());
        logFontExA.elfDesignVector = pLogFontExW->elfDesignVector;
    }

    TEXTMETRICA textMetricA = ConvertTextMetricWToA(*lptm);
    return pContext->OriginalProc(&logFontExA.elfEnumLogfontEx.elfLogFont, &textMetricA, dwType, pContext->OriginalContext);
}

inline const wchar_t* charsetToStr(BYTE cs) {
    switch (cs) {
    case ANSI_CHARSET: return L"ANSI";
    case DEFAULT_CHARSET: return L"DEFAULT";
    case SYMBOL_CHARSET: return L"SYMBOL";
    case SHIFTJIS_CHARSET: return L"SHIFTJIS";
    case HANGUL_CHARSET: return L"HANGUL";
    case JOHAB_CHARSET: return L"JOHAB";
    case GB2312_CHARSET: return L"GB2312";
    case CHINESEBIG5_CHARSET: return L"BIG5";
    case GREEK_CHARSET: return L"GREEK";
    case TURKISH_CHARSET: return L"TURKISH";
    case HEBREW_CHARSET: return L"HEBREW";
    case ARABIC_CHARSET: return L"ARABIC";
    case BALTIC_CHARSET: return L"BALTIC";
    case RUSSIAN_CHARSET: return L"CYRILLIC";
    case THAI_CHARSET: return L"THAI";
    case EASTEUROPE_CHARSET: return L"EASTEUROPE";
    case VIETNAMESE_CHARSET: return L"VIETNAMESE";
    case MAC_CHARSET: return L"MAC";
    case OEM_CHARSET: return L"OEM";
    default: return L"(unknown)";
    }
}

inline const wchar_t* outPrecToStr(BYTE p) {
    switch (p) {
    case OUT_DEFAULT_PRECIS: return L"DEFAULT";
    case OUT_STRING_PRECIS: return L"STRING";
    case OUT_CHARACTER_PRECIS: return L"CHARACTER";
    case OUT_STROKE_PRECIS: return L"STROKE";
    case OUT_TT_PRECIS: return L"TRUETYPE";
    case OUT_DEVICE_PRECIS: return L"DEVICE";
    case OUT_RASTER_PRECIS: return L"RASTER";
    case OUT_TT_ONLY_PRECIS: return L"TT_ONLY";
    case OUT_OUTLINE_PRECIS: return L"OUTLINE";
    case OUT_SCREEN_OUTLINE_PRECIS: return L"SCREEN_OUTLINE";
    case OUT_PS_ONLY_PRECIS: return L"POSTSCRIPT_ONLY";
    default: return L"(unknown)";
    }
}

inline std::wstring clipPrecToStr(BYTE p) {
    std::wstringstream ss;
    BYTE base = p & 0x0F;
    switch (base) {
    case CLIP_DEFAULT_PRECIS: ss << L"DEFAULT"; break;
    case CLIP_CHARACTER_PRECIS: ss << L"CHARACTER"; break;
    case CLIP_STROKE_PRECIS: ss << L"STROKE"; break;
    default: ss << L"(unknown-base:" << int(base) << L")"; break;
    }
    // flags
    if (p & CLIP_LH_ANGLES)     ss << L" | LH_ANGLES";
    if (p & CLIP_TT_ALWAYS)     ss << L" | TT_ALWAYS";
    if (p & CLIP_DFA_DISABLE)   ss << L" | DFA_DISABLE";
    if (p & CLIP_EMBEDDED)      ss << L" | EMBEDDED";
    return ss.str();
}

inline const wchar_t* qualityToStr(BYTE q) {
    switch (q) {
    case DEFAULT_QUALITY: return L"DEFAULT";
    case DRAFT_QUALITY: return L"DRAFT";
    case PROOF_QUALITY: return L"PROOF";
    case NONANTIALIASED_QUALITY: return L"NONANTIALIASED";
    case ANTIALIASED_QUALITY: return L"ANTIALIASED";
    case CLEARTYPE_QUALITY: return L"CLEARTYPE";
    case CLEARTYPE_NATURAL_QUALITY: return L"CLEARTYPE_NATURAL";
    default: return L"(unknown)";
    }
}

inline const wchar_t* weightToName(LONG w) {
    switch (w) {
    case FW_THIN: return L"THIN (100)";
    case FW_EXTRALIGHT: return L"EXTRALIGHT (200)";
    case FW_LIGHT: return L"LIGHT (300)";
    case FW_NORMAL: return L"NORMAL (400)";
    case FW_MEDIUM: return L"MEDIUM (500)";
    case FW_SEMIBOLD: return L"SEMIBOLD (600)";
    case FW_BOLD: return L"BOLD (700)";
    case FW_EXTRABOLD: return L"EXTRABOLD (800)";
    case FW_HEAVY: return L"HEAVY (900)";
    default: return L"(custom)";
    }
}

inline std::wstring pitchFamilyToStr(BYTE pf) {
    std::wstringstream ss;
    BYTE pitch = pf & 0x03;
    BYTE fam = pf & 0xF0;

    switch (pitch) {
    case DEFAULT_PITCH:  ss << L"DEFAULT_PITCH"; break;
    case FIXED_PITCH:    ss << L"FIXED_PITCH"; break;
    case VARIABLE_PITCH: ss << L"VARIABLE_PITCH"; break;
    default:             ss << L"(pitch?" << int(pitch) << L")"; break;
    }
    ss << L", ";

    switch (fam) {
    case FF_DONTCARE:   ss << L"FF_DONTCARE"; break;
    case FF_ROMAN:      ss << L"FF_ROMAN"; break;
    case FF_SWISS:      ss << L"FF_SWISS"; break;
    case FF_MODERN:     ss << L"FF_MODERN"; break;
    case FF_SCRIPT:     ss << L"FF_SCRIPT"; break;
    case FF_DECORATIVE: ss << L"FF_DECORATIVE"; break;
    default:            ss << L"(family?" << int(fam) << L")"; break;
    }
    return ss.str();
}

inline double tenthsToDegrees(LONG v) {
    // lfEscapement / lfOrientation are in tenths of degrees.
    return static_cast<double>(v) / 10.0;
}

inline std::wstring explainHeight(LONG h) {
    std::wstringstream ss;
    ss << h << L" (";
    if (h < 0)
        ss << L"character height = " << -h << L" logical units";
    else if (h > 0)
        ss << L"cell height = " << h << L" logical units";
    else
        ss << L"height = 0 (use default)";
    ss << L")";
    return ss.str();
}

HFONT GdiProportionalizer::CreateFontAHook(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
    DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        // Convert strings from wide pretty-printers to UTF-8 for fprintf
        auto toUtf8 = [](const std::wstring& ws) {
            if (ws.empty()) return std::string();
            int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string out(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), size, nullptr, nullptr);
            if (!out.empty() && out.back() == '\0') out.pop_back(); // strip null terminator
            return out;
            };
        auto toUtf8C = [&](const wchar_t* ws) {
            return toUtf8(std::wstring(ws));
            };

        fprintf(log, "GdiProportionalizer::CreateFontAHook() called with:\n");
        fprintf(log, "  cHeight        = %s\n", toUtf8(explainHeight(cHeight)).c_str());
        fprintf(log, "  cWidth         = %d\n", cWidth);
        fprintf(log, "  cEscapement    = %d (%.1f°)\n", cEscapement, tenthsToDegrees(cEscapement));
        fprintf(log, "  cOrientation   = %d (%.1f°)\n", cOrientation, tenthsToDegrees(cOrientation));
        fprintf(log, "  cWeight        = %d (%s)\n", cWeight, toUtf8C(weightToName(cWeight)).c_str());
        fprintf(log, "  bItalic        = %lu\n", bItalic);
        fprintf(log, "  bUnderline     = %lu\n", bUnderline);
        fprintf(log, "  bStrikeOut     = %lu\n", bStrikeOut);
        fprintf(log, "  iCharSet       = %lu (%s)\n", iCharSet, toUtf8C(charsetToStr((BYTE)iCharSet)).c_str());
        fprintf(log, "  iOutPrecision  = %lu (%s)\n", iOutPrecision, toUtf8C(outPrecToStr((BYTE)iOutPrecision)).c_str());
        fprintf(log, "  iClipPrecision = %lu (%s)\n", iClipPrecision, toUtf8(clipPrecToStr((BYTE)iClipPrecision)).c_str());
        fprintf(log, "  iQuality       = %lu (%s)\n", iQuality, toUtf8C(qualityToStr((BYTE)iQuality)).c_str());
        fprintf(log, "  iPitchAndFamily= %lu (%s)\n", iPitchAndFamily, toUtf8(pitchFamilyToStr((BYTE)iPitchAndFamily)).c_str());
        fprintf(log, "  pszFaceName    = %s\n", pszFaceName ? pszFaceName : "(null)");
        fprintf(log, "------------------------------------------------------\n\n");

        fclose(log);
    }
#endif

    return CreateFontWHook(
        cHeight,
        cWidth,
        cEscapement,
        cOrientation,
        cWeight,
        bItalic,
        bUnderline,
        bStrikeOut,
        iCharSet,
        iOutPrecision,
        iClipPrecision,
        iQuality,
        iPitchAndFamily,
        StringUtil::ToWString(pszFaceName).c_str()
    );
}

HFONT GdiProportionalizer::CreateFontIndirectAHook(LOGFONTA* pFontInfo)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::CreateFontIndirectAHook() \n");
        fclose(log);
    }
#endif

    return CreateFontWHook(
        pFontInfo->lfHeight,
        pFontInfo->lfWidth,
        pFontInfo->lfEscapement,
        pFontInfo->lfOrientation,
        pFontInfo->lfWeight,
        pFontInfo->lfItalic,
        pFontInfo->lfUnderline,
        pFontInfo->lfStrikeOut,
        pFontInfo->lfCharSet,
        pFontInfo->lfOutPrecision,
        pFontInfo->lfClipPrecision,
        pFontInfo->lfQuality,
        pFontInfo->lfPitchAndFamily,
        StringUtil::ToWString(pFontInfo->lfFaceName).c_str()
    );
}

HFONT GdiProportionalizer::CreateFontWHook(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
    DWORD iQuality, DWORD iPitchAndFamily, LPCWSTR pszFaceName)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::CreateFontWHook() \n");
        fclose(log);
    }
#endif

    LOGFONTW fontInfo;
    fontInfo.lfHeight = cHeight;
    fontInfo.lfWidth = cWidth;
    fontInfo.lfEscapement = cEscapement;
    fontInfo.lfOrientation = cOrientation;
    fontInfo.lfWeight = cWeight;
    fontInfo.lfItalic = bItalic;
    fontInfo.lfUnderline = bUnderline;
    fontInfo.lfStrikeOut = bStrikeOut;
    fontInfo.lfCharSet = iCharSet;
    fontInfo.lfOutPrecision = iOutPrecision;
    fontInfo.lfClipPrecision = iClipPrecision;
    fontInfo.lfQuality = iQuality;
    fontInfo.lfPitchAndFamily = iPitchAndFamily;
    wcscpy_s(fontInfo.lfFaceName, pszFaceName);
    return CreateFontIndirectWHook(&fontInfo);
}

HFONT GdiProportionalizer::CreateFontIndirectWHook(LOGFONTW* pFontInfo)
{

    if (CustomFontName.empty())
    {
        LastFontName = pFontInfo->lfFaceName;
        return FontManager.FetchFont(*pFontInfo)->GetGdiHandle();
    }

    LastFontName = CustomFontName;

#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::CreateFontIndirectWHook(): CustomFontName: %ls, height: %d \n", CustomFontName.c_str(), pFontInfo->lfHeight);
        fclose(log);
    }
#endif

    return FontManager.FetchFont(CustomFontName, pFontInfo->lfHeight, Bold, Italic, Underline)->GetGdiHandle();
}

HGDIOBJ GdiProportionalizer::SelectObjectHook(HDC hdc, HGDIOBJ obj)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::SelectObjectHook()\n");
        fclose(log);
    }
#endif
    state.penXF = 0.0;
    state.penXI = 0;
    state.carry = 0.0;
    state.ACarry = 0.0;
    state.prevCh = 0xFFFF;

    Font* pFont = FontManager.GetFont(static_cast<HFONT>(obj));
    if (pFont != nullptr)
        CurrentFonts[hdc] = pFont;

    HGDIOBJ ret = SelectObject(hdc, obj);

    DWORD count = GetKerningPairsW(hdc, 0, nullptr);
    if (count == 0) {
#if _DEBUG
        FILE* log = nullptr;
        if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
            fprintf(log, "A: 0 kerning pairs\n");
            fclose(log);
        }
#endif
        // No pairs or error; optionally check GetLastError()
        return ret;
    }

    // Allocate and fetch the pairs
    std::vector<KERNINGPAIR> pairs(count);
    DWORD got = GetKerningPairsW(hdc, count, pairs.data());
    if (got == 0) {
#if _DEBUG
        FILE* log = nullptr;
        if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
            fprintf(log, "B: 0 kerning pairs\n");
            fclose(log);
        }
#endif
        // Failed; optionally check GetLastError()
        return ret;
    }

    // Store as key = wFirst | (wSecond << 16), value = iKernAmount
    kernAmounts.reserve(kernAmounts.size() + got);
    for (DWORD i = 0; i < got; ++i) {
        const KERNINGPAIR& kp = pairs[i];
/*#if _DEBUG
        FILE* log = nullptr;
        if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
            fprintf(log, "Kerning pair: %d, %d, %d\n", kp.wFirst, kp.wSecond, kp.iKernAmount);
            fclose(log);
        }
#endif*/
        uint32_t key = static_cast<uint32_t>(kp.wFirst)
            | (static_cast<uint32_t>(kp.wSecond) << 16);
        kernAmounts[key] = static_cast<int>(kp.iKernAmount);
    }

    return ret;
}

BOOL GdiProportionalizer::DeleteObjectHook(HGDIOBJ obj)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::DeleteObjectHook() \n");
        fclose(log);
    }
#endif

    Font* pFont = FontManager.GetFont(static_cast<HFONT>(obj));
    if (pFont != nullptr)
        return false;

    return DeleteObject(obj);
}

BOOL GdiProportionalizer::GetTextExtentPointAHook(HDC hdc, LPCSTR lpString, int c, LPSIZE lpsz)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::GetTextExtentPointAHook() \n");
        fclose(log);
    }
#endif

    wstring str = SjisTunnelEncoding::Decode(lpString, c);
    return GetTextExtentPointW(hdc, str.c_str(), str.size(), lpsz);
}

BOOL GdiProportionalizer::GetTextExtentPoint32AHook(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::GetTextExtentPoint32AHook() \n");
        fclose(log);
    }
#endif

    wstring str = SjisTunnelEncoding::Decode(lpString, c);
    return GetTextExtentPoint32W(hdc, str.c_str(), str.size(), psizl);
}

BOOL GdiProportionalizer::TextOutAHook(HDC dc, int x, int y, LPCSTR pString, int count)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::TextOutAHook() \n");
        fclose(log);
    }
#endif
    
    wstring text = SjisTunnelEncoding::Decode(pString, count);
    Font* pFont = CurrentFonts[dc];
    if (pFont == nullptr)
    {
        pFont = FontManager.FetchFont(L"System", 12, false, false, false);
        SelectObjectHook(dc, pFont->GetGdiHandle());
    }

    if (!AdaptRenderArgs(text.c_str(), text.size(), pFont->GetHeight(), x, y))
        return false;

    if (!CustomFontName.empty() && (pFont->IsBold() != Bold || pFont->IsItalic() != Italic || pFont->IsUnderline() != Underline))
    {
        pFont = FontManager.FetchFont(CustomFontName, pFont->GetHeight(), Bold, Italic, Underline);
        SelectObjectHook(dc, pFont->GetGdiHandle());
    }

    return TextOutW(dc, x, y, text.data(), text.size());
}

std::string FuFormatToString(UINT fuFormat)
{
    std::ostringstream oss;

    // --- Main formats (mutually exclusive) ---
    switch (fuFormat & 0xFF) { // low byte stores the main mode
    case GGO_METRICS:       oss << "GGO_METRICS"; break;
    case GGO_BITMAP:        oss << "GGO_BITMAP"; break;
    case GGO_NATIVE:        oss << "GGO_NATIVE"; break;
    case GGO_BEZIER:        oss << "GGO_BEZIER"; break;
    case GGO_GRAY2_BITMAP:  oss << "GGO_GRAY2_BITMAP"; break;
    case GGO_GRAY4_BITMAP:  oss << "GGO_GRAY4_BITMAP"; break;
    case GGO_GRAY8_BITMAP:  oss << "GGO_GRAY8_BITMAP"; break;
    default:                oss << "UNKNOWN_FORMAT(" << (fuFormat & 0xFF) << ")"; break;
    }

    // --- Option flags (may combine) ---
    if (fuFormat & GGO_GLYPH_INDEX)
        oss << " | GGO_GLYPH_INDEX";

    if (fuFormat & GGO_UNHINTED)
        oss << " | GGO_UNHINTED";

    return oss.str();
}


std::string GlyphMetricsToString(const GLYPHMETRICS* gm)
{
    if (!gm) return "NULL";

    std::ostringstream oss;
    oss << "GLYPHMETRICS { "
        << "gmBlackBoxX=" << gm->gmBlackBoxX << ", "
        << "gmBlackBoxY=" << gm->gmBlackBoxY << ", "
        << "gmptGlyphOrigin=("
        << gm->gmptGlyphOrigin.x << ", "
        << gm->gmptGlyphOrigin.y << "), "
        << "gmCellIncX=" << gm->gmCellIncX << ", "
        << "gmCellIncY=" << gm->gmCellIncY
        << " }";

    return oss.str();
}

// Shift a GGO_GRAY2_BITMAP buffer horizontally by dx columns.
// Positive dx => shift ink to the RIGHT (adds blank columns on the left).
// Negative dx => shift ink to the LEFT  (adds blank columns on the right).
//
// - lpv: pointer to the glyph bitmap buffer returned by GetGlyphOutline* (GRAY2).
// - gm:  GLYPHMETRICS for this glyph (used for width/height/stride). We do NOT modify it.
// - dx:  integer shift (will be clamped to [-gm->gmBlackBoxX, gm->gmBlackBoxX]).
//
// Notes:
// - Works in-place; uses a small temporary row buffer on the stack.
// - Assumes format == GGO_GRAY2_BITMAP (2 bits per pixel, leftmost pixel is in the high-order bits).
// - Keeps gm->gmBlackBoxX unchanged (we shift ink within the existing box).
// - If you also adjust gm->gmptGlyphOrigin.x outside this function, you can "nudge" placement
//   without changing the visible width.
static void ShiftGrayBitmapHoriz(void* lpv, const GLYPHMETRICS* gm, int dx)
{
    if (!lpv || !gm) return;

    const int w = (int)gm->gmBlackBoxX;
    const int h = (int)gm->gmBlackBoxY;
    if (w <= 0 || h <= 0 || dx == 0) return;

    // Clamp dx to avoid reading outside the row.
    if (dx > w) dx = w;
    if (dx < -w) dx = -w;

    // 2-bpp, DWORD-aligned stride per GDI docs.
    // bitsPerRow = 2 * w; strideBytes = alignUp(bitsPerRow, 32)/8
    const int bitsPerRow = 2 * w;
    const int strideBytes = ((bitsPerRow + 31) & ~31) >> 3;

    // Temporary row buffers (unpacked/packed).
    // We keep 1 byte per pixel for simplicity; w for src and w for dst is enough.
    // If your max glyph width can be very large, consider heap allocation.
    std::vector<uint8_t> row(w);
    std::vector<uint8_t> shifted(w);

    uint8_t* base = static_cast<uint8_t*>(lpv);

    for (int y = 0; y < h; ++y) {
        uint8_t* rowBytes = base + y * strideBytes;

        // Unpack 2-bpp to 1-byte-per-pixel (values 0..3).
        // GDI packs leftmost pixel in the high-order bits of the first byte.
        for (int i = 0; i < w; ++i) {
            const int byteIndex = i >> 2;                 // 4 pixels per byte
            const int shift = 6 - ((i & 3) * 2);      // 6,4,2,0
            row[i] = (rowBytes[byteIndex] >> shift) & 0x3;
        }

        // Shift: destination pixel i comes from source (i - dx).
        // Anything falling outside [0, w-1] becomes 0 (blank).
        if (dx != 0) {
            for (int i = 0; i < w; ++i) {
                const int src = i - dx;
                shifted[i] = (src >= 0 && src < w) ? row[src] : 0;
            }
        }
        else {
            // (Never hit, as dx==0 is early-returned; here for completeness.)
            std::copy(row.begin(), row.end(), shifted.begin());
        }

        // Clear the whole stride so unused bits don’t carry garbage.
        std::memset(rowBytes, 0, (size_t)strideBytes);

        // Repack from 0..3 to 2-bpp, high bits first.
        for (int i = 0; i < w; ++i) {
            const int byteIndex = i >> 2;
            const int shift = 6 - ((i & 3) * 2);
            rowBytes[byteIndex] |= (uint8_t)((shifted[i] & 0x3) << shift);
        }
        // Any padding bytes at end of the stride remain 0.
    }
}


DWORD GdiProportionalizer::GetGlyphOutlineAHook(HDC hdc, UINT uChar, UINT fuFormat, LPGLYPHMETRICS lpgm, DWORD cjBuffer, LPVOID pvBuffer, MAT2* lpmat2)
{
    string str;
    while (uChar != 0)
    {
        str.insert(0, 1, (char)uChar);
        uChar >>= 8;
    }
    wstring wstr = SjisTunnelEncoding::Decode(str);

    UINT ch = wstr[0];

    DWORD ret = GetGlyphOutlineW(hdc, ch, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);

    //    if (lpgm->gmCellIncX > 3)
    //      lpgm->gmCellIncX -= 3;

    if (wstr[0] == 'm') {
        //        lpgm->gmBlackBoxX += 5;
        //        lpgm->gmptGlyphOrigin.x -= 2;
    }

    ABCFLOAT abc;
    GetCharABCWidthsFloatW(hdc, ch, ch, &abc);

    uint32_t kernKey = static_cast<uint32_t>(state.prevCh) | (static_cast<uint32_t>(ch) << 16);
    double kern = kernAmounts[kernKey];

//    double kern = 0.0;

    // 2) Integer-approx the floating advance
    double advanceF = abc.abcfA + abc.abcfB + abc.abcfC + kern;
    double withCarry = advanceF + state.carry;
    int    advOut = (int)floor(withCarry + 0.5);
    state.carry = withCarry - advOut;

    // 3)  A nudge
    int nudge = 0;
    double aWithCarry = abc.abcfA - floor(abc.abcfA) + state.ACarry;
    nudge = (int)floor(aWithCarry + 0.5);                   // −1, 0, +1 in practice
    if (pvBuffer != NULL) {
        state.ACarry = aWithCarry - nudge;
    }

#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::GetGlyphOutlineAHook() fuFormat: %s, char: %s, 0x%x, pvBuffer: %d, cjBuffer: %d, metricsResult: %s, nudge: %d, advOut: %d, carry: %f, ACarry: %f, a: %f, b: %f, c: %f, kern: %f\n", FuFormatToString(fuFormat).c_str(), reinterpret_cast<const char*>(wstr.c_str()), wstr[0], pvBuffer != NULL, cjBuffer, GlyphMetricsToString(lpgm).c_str(), nudge, advOut, state.carry, state.ACarry, abc.abcfA, abc.abcfB, abc.abcfC, kern);
        fclose(log);
    }
#endif

    // Shift bitmap horizontally by `nudge` cols (add/remove blank cols)
    ShiftGrayBitmapHoriz(pvBuffer, lpgm, nudge);               // updates lpv + gmSys.gmBlackBoxX

    // Move origin so caller draws it where we intend
    lpgm->gmptGlyphOrigin.x += nudge;

    if (wstr[0] == '|') {
        advOut -= 3;
        if (pvBuffer && cjBuffer >= ret) {
            memset(pvBuffer, 0, ret);
        }
    }
    else if (wstr[0] == 'F' || wstr[0] == 'e') {
        advOut -= 1;
    }
    else {
        advOut -= 1;
    }

    // 4) Publish adjusted metrics to the caller
//    *lpgm = gmSys;
    lpgm->gmCellIncX = advOut;

    // 5) Advance your internal pens for next call
    if (pvBuffer != NULL) {
        state.penXF += advanceF;
        state.penXI += advOut;
        state.prevCh = ch;
    }

    return ret;
}

LOGFONTA GdiProportionalizer::ConvertLogFontWToA(const LOGFONTW& logFontW)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::ConvertLogFontWToA() \n");
        fclose(log);
    }
#endif

    LOGFONTA logFontA;
    logFontA.lfCharSet = logFontW.lfCharSet;
    logFontA.lfClipPrecision = logFontW.lfClipPrecision;
    logFontA.lfEscapement = logFontW.lfEscapement;
    strcpy_s(logFontA.lfFaceName, SjisTunnelEncoding::Encode(logFontW.lfFaceName).c_str());
    logFontA.lfHeight = logFontW.lfHeight;
    logFontA.lfItalic = logFontW.lfItalic;
    logFontA.lfOrientation = logFontW.lfOrientation;
    logFontA.lfOutPrecision = logFontW.lfOutPrecision;
    logFontA.lfPitchAndFamily = logFontW.lfPitchAndFamily;
    logFontA.lfQuality = logFontW.lfQuality;
    logFontA.lfStrikeOut = logFontW.lfStrikeOut;
    logFontA.lfUnderline = logFontW.lfUnderline;
    logFontA.lfWeight = logFontW.lfWeight;
    logFontA.lfWidth = logFontW.lfWidth;
    return logFontA;
}

LOGFONTW GdiProportionalizer::ConvertLogFontAToW(const LOGFONTA& logFontA)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::ConvertLogFontAToW() \n");
        fclose(log);
    }
#endif

    LOGFONTW logFontW;
    logFontW.lfCharSet = logFontA.lfCharSet;
    logFontW.lfClipPrecision = logFontA.lfClipPrecision;
    logFontW.lfEscapement = logFontA.lfEscapement;
    wcscpy_s(logFontW.lfFaceName, SjisTunnelEncoding::Decode(logFontA.lfFaceName).c_str());
    logFontW.lfHeight = logFontA.lfHeight;
    logFontW.lfItalic = logFontA.lfItalic;
    logFontW.lfOrientation = logFontA.lfOrientation;
    logFontW.lfOutPrecision = logFontA.lfOutPrecision;
    logFontW.lfPitchAndFamily = logFontA.lfPitchAndFamily;
    logFontW.lfQuality = logFontA.lfQuality;
    logFontW.lfStrikeOut = logFontA.lfStrikeOut;
    logFontW.lfUnderline = logFontA.lfUnderline;
    logFontW.lfWeight = logFontA.lfWeight;
    logFontW.lfWidth = logFontA.lfWidth;
    return logFontW;
}

TEXTMETRICA GdiProportionalizer::ConvertTextMetricWToA(const TEXTMETRICW& textMetricW)
{
#if _DEBUG
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::ConvertTextMetricWToA() \n");
        fclose(log);
    }
#endif

    TEXTMETRICA textMetricA;
    textMetricA.tmAscent = textMetricW.tmAscent;
    textMetricA.tmAveCharWidth = textMetricW.tmAveCharWidth;
    textMetricA.tmBreakChar = textMetricW.tmBreakChar < 0x100 ? (BYTE)textMetricW.tmBreakChar : '?';
    textMetricA.tmCharSet = textMetricW.tmCharSet;
    textMetricA.tmDefaultChar = textMetricW.tmDefaultChar < 0x100 ? (BYTE)textMetricW.tmDefaultChar : '?';
    textMetricA.tmDescent = textMetricW.tmDescent;
    textMetricA.tmDigitizedAspectX = textMetricW.tmDigitizedAspectX;
    textMetricA.tmDigitizedAspectY = textMetricW.tmDigitizedAspectY;
    textMetricA.tmExternalLeading = textMetricW.tmExternalLeading;
    textMetricA.tmFirstChar = (BYTE)min(textMetricW.tmFirstChar, 0xFF);
    textMetricA.tmHeight = textMetricW.tmHeight;
    textMetricA.tmInternalLeading = textMetricW.tmInternalLeading;
    textMetricA.tmItalic = textMetricW.tmItalic;
    textMetricA.tmLastChar = (BYTE)min(textMetricW.tmLastChar, 0xFF);
    textMetricA.tmMaxCharWidth = textMetricW.tmMaxCharWidth;
    textMetricA.tmOverhang = textMetricW.tmOverhang;
    textMetricA.tmPitchAndFamily = textMetricW.tmPitchAndFamily;
    textMetricA.tmStruckOut = textMetricW.tmStruckOut;
    textMetricA.tmUnderlined = textMetricW.tmUnderlined;
    textMetricA.tmWeight = textMetricW.tmWeight;
    return textMetricA;
}
