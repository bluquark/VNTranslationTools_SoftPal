#include <string>
#include <sstream>
#include <iomanip>

#include "pch.h"

#include "PALHooks.h"

#define GDI_LOGGING 0

using namespace std;

/*
SoftPal uses GDI methods only to create the font and to compute the metrics and bitmap of one character at a time.
Then on its end (and beyond our control here), it adds a black outline around the bitmap and prints it onscreen itself.
It uses the GDI-returned gmCellIncX to advance its internal "pen position".
Since GDI never sees more than one letter per function call, its native kerning support doesn't do anything.

Example call sequence from SoftPal:

GdiProportionalizer::CreateFontAHook()
GdiProportionalizer::CreateFontWHook()
GdiProportionalizer::CreateFontIndirectWHook()
GdiProportionalizer::SelectObjectHook()
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
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::GetGlyphOutlineAHook() char: ?, 0x3f
GdiProportionalizer::SelectObjectHook()
GdiProportionalizer::DeleteObjectHook()
*/

void GdiProportionalizer::Init()
{
#if GDI_LOGGING
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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

HFONT GdiProportionalizer::CreateFontAHook(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight,
    DWORD bItalic, DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
    DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
#if GDI_LOGGING
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::CreateFontWHook() \n");
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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
    fontInfo.lfItalic = (BYTE) bItalic;
    fontInfo.lfUnderline = (BYTE) bUnderline;
    fontInfo.lfStrikeOut = (BYTE) bStrikeOut;
    fontInfo.lfCharSet = (BYTE) iCharSet;
    fontInfo.lfOutPrecision = (BYTE) iOutPrecision;
    fontInfo.lfClipPrecision = (BYTE) iClipPrecision;
    fontInfo.lfQuality = (BYTE) iQuality;
    fontInfo.lfPitchAndFamily = (BYTE) iPitchAndFamily;
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

#if GDI_LOGGING
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::CreateFontIndirectWHook(): CustomFontName: %ls, height: %d \n", CustomFontName.c_str(), pFontInfo->lfHeight);
        fclose(log);
    }
#endif

    return FontManager.FetchFont(CustomFontName, pFontInfo->lfHeight, Bold, Italic, Underline)->GetGdiHandle();
}

int totalAdvOut = 0;

HGDIOBJ GdiProportionalizer::SelectObjectHook(HDC hdc, HGDIOBJ obj)
{
#if GDI_LOGGING
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::SelectObjectHook(): currentText: %s\n", PALGrabCurrentText::get().c_str());
        fclose(log);
    }
#endif
    Font* pFont = FontManager.GetFont(static_cast<HFONT>(obj));
    if (pFont != nullptr)
        CurrentFonts[hdc] = pFont;

    totalAdvOut = 0;

    return SelectObject(hdc, obj);
}

BOOL GdiProportionalizer::DeleteObjectHook(HGDIOBJ obj)
{
#if GDI_LOGGING
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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

    // Special workaround to make '{' behave as if it were a percentage character.
    // This code is intended to reverse the '%' -> '{' replacement in SoftpalScript.WritePatched().
    // (We can't use % characters in TEXT.DAT because they're used to display non-SJIS special characters, for example %0 is rendered as ❤.)
    if (ch == '{') {
        ch = '%';
    }

    DWORD ret = GetGlyphOutlineW(hdc, ch, fuFormat, lpgm, cjBuffer, pvBuffer, lpmat2);

    // Special workaround to make '|' behave as if it were a space.
    // This code is intended to reverse the ' ' -> '|' replacement in SoftpalScript.WritePatched().
    // (We can't use space characters in TEXT.DAT because SoftPal hardcodes an advance distance for them.)
    if (ch == '|') {
        ch = ' ';

        // Wipe all pixels from the bitmap GetGlyphOutlineW created
        if (pvBuffer && cjBuffer >= ret) {
            memset(pvBuffer, 0, ret);
        }
    }

    // TODO: Populate this value
    int kern = 0;

    ABCFLOAT abc;
    GetCharABCWidthsFloatW(hdc, ch, ch, &abc);
    double advanceF = abc.abcfA + abc.abcfB + abc.abcfC + kern;
    int advOut = (int)floor(advanceF + 0.5);
    
    if (pvBuffer) {
        totalAdvOut += advOut;
    }

#if GDI_LOGGING
    FILE* log = nullptr;
    if (fopen_s(&log, "winmm_dll_log.txt", "at") == 0 && log) {
        fprintf(log, "GdiProportionalizer::GetGlyphOutlineAHook() fuFormat: %s, char: %s, 0x%x, pvBuffer: %d, cjBuffer: %d, metricsResult: %s, advOut: %d, totalAdvOut: %d, a: %f, b: %f, c: %f, kern: %d\n", FuFormatToString(fuFormat).c_str(), reinterpret_cast<const char*>(wstr.c_str()), wstr[0], pvBuffer != NULL, cjBuffer, GlyphMetricsToString(lpgm).c_str(), advOut, totalAdvOut, abc.abcfA, abc.abcfB, abc.abcfC, kern);
        fclose(log);
    }
#endif

    // SoftPal systematically adds 1 extra pixel of spacing after every character, beyond what the font specifies.
    // This is not very noticeable with Japanese characters, but it's extremely noticeable with a proportional Latin font.
    // Cancel out that behavior here.
    if (advOut > 0) {
        advOut -= 1;
    }

    lpgm->gmCellIncX = advOut;

    return ret;
}

LOGFONTA GdiProportionalizer::ConvertLogFontWToA(const LOGFONTW& logFontW)
{
#if GDI_LOGGING
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
#if GDI_LOGGING
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
#if GDI_LOGGING
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
