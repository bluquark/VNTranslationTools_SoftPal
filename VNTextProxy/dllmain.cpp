#include "pch.h"

#include "SharedConstants.h"
#include "PALHooks.h"
#include "DX9Hooks.h"
#include "DX11Hooks.h"
#include "Util/Logger.h"
#include <sstream>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

void* OriginalEntryPoint;

static void LogDirectoryListing(const char* label, const wchar_t* searchPattern)
{
    proxy_log(LogCategory::INIT, "--- %s ---", label);

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        proxy_log(LogCategory::INIT, "  (directory not found or empty)");
        return;
    }

    do
    {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
            continue;

        SYSTEMTIME st;
        FileTimeToSystemTime(&findData.ftLastWriteTime, &st);

        bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        char nameBuf[260];
        WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);

        if (isDir)
        {
            proxy_log(LogCategory::INIT, "  %04d-%02d-%02d %02d:%02d:%02d  <DIR>          %s",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, nameBuf);
        }
        else
        {
            ULARGE_INTEGER fileSize;
            fileSize.LowPart = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            proxy_log(LogCategory::INIT, "  %04d-%02d-%02d %02d:%02d:%02d  %13llu  %s",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                fileSize.QuadPart, nameBuf);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static bool IsFatalExceptionCode(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_IN_PAGE_ERROR:
        case EXCEPTION_PRIV_INSTRUCTION:
            return true;
        default:
            return false;
    }
}

static void LogFrameAddress(int index, DWORD addr)
{
    char moduleName[MAX_PATH] = "???";
    DWORD moduleOffset = 0;

    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)(DWORD_PTR)addr, &hMod))
    {
        wchar_t modPath[MAX_PATH];
        GetModuleFileNameW(hMod, modPath, MAX_PATH);
        const wchar_t* modName = wcsrchr(modPath, L'\\');
        modName = modName ? modName + 1 : modPath;
        WideCharToMultiByte(CP_UTF8, 0, modName, -1, moduleName, sizeof(moduleName), nullptr, nullptr);
        moduleOffset = (DWORD)((DWORD_PTR)addr - (DWORD_PTR)hMod);
    }

    proxy_log(LogCategory::FATAL, "    [%2d] %s + 0x%08X", index, moduleName, moduleOffset);
}

static void LogCallStack(CONTEXT* ctx)
{
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(hProcess, nullptr, TRUE);

    CONTEXT ctxCopy = *ctx;

    // If EIP is 0 (null function pointer call), recover the caller from the stack.
    // A "call" pushes the return address onto ESP, so [ESP] is the caller.
    int startIndex = 0;
    if (ctxCopy.Eip == 0)
    {
        __try
        {
            DWORD returnAddr = *(DWORD*)(DWORD_PTR)ctxCopy.Esp;
            if (returnAddr != 0)
            {
                proxy_log(LogCategory::FATAL, "  Call stack (EIP was 0, recovered from ESP):");
                LogFrameAddress(0, returnAddr);
                startIndex = 1;
                ctxCopy.Eip = returnAddr;
                ctxCopy.Esp += 4;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            proxy_log(LogCategory::FATAL, "  Call stack (EIP was 0, could not read ESP):");
            SymCleanup(hProcess);
            return;
        }
    }
    else
    {
        proxy_log(LogCategory::FATAL, "  Call stack:");
    }

    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = ctxCopy.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctxCopy.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctxCopy.Esp;
    frame.AddrStack.Mode = AddrModeFlat;

    for (int i = startIndex; i < 64; i++)
    {
        if (!StackWalk64(IMAGE_FILE_MACHINE_I386, hProcess, hThread,
                         &frame, &ctxCopy, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;

        if (frame.AddrPC.Offset == 0)
            break;

        LogFrameAddress(i, (DWORD)frame.AddrPC.Offset);
    }

    SymCleanup(hProcess);
}

static LONG CALLBACK VectoredCrashHandler(EXCEPTION_POINTERS* ep)
{
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (!IsFatalExceptionCode(er->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    CONTEXT* ctx = ep->ContextRecord;

    const char* exName;
    switch (er->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:    exName = "ACCESS_VIOLATION"; break;
        case EXCEPTION_STACK_OVERFLOW:      exName = "STACK_OVERFLOW"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:  exName = "INT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: exName = "ILLEGAL_INSTRUCTION"; break;
        case EXCEPTION_IN_PAGE_ERROR:       exName = "IN_PAGE_ERROR"; break;
        case EXCEPTION_PRIV_INSTRUCTION:    exName = "PRIV_INSTRUCTION"; break;
        default:                            exName = "UNKNOWN"; break;
    }

    proxy_log(LogCategory::FATAL, "Unhandled exception %s (0x%08X) at 0x%08X",
        exName, er->ExceptionCode, (DWORD)(DWORD_PTR)er->ExceptionAddress);

    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
    {
        const char* op = er->ExceptionInformation[0] == 0 ? "Read" :
                         er->ExceptionInformation[0] == 1 ? "Write" : "DEP violation";
        proxy_log(LogCategory::FATAL, "  %s of address 0x%08X", op, (DWORD)er->ExceptionInformation[1]);
    }

    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)er->ExceptionAddress, &hMod))
    {
        wchar_t modPath[MAX_PATH];
        GetModuleFileNameW(hMod, modPath, MAX_PATH);
        const wchar_t* modName = wcsrchr(modPath, L'\\');
        modName = modName ? modName + 1 : modPath;
        DWORD offset = (DWORD)((DWORD_PTR)er->ExceptionAddress - (DWORD_PTR)hMod);
        char modNameBuf[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, modName, -1, modNameBuf, sizeof(modNameBuf), nullptr, nullptr);
        proxy_log(LogCategory::FATAL, "  Module: %s + 0x%08X", modNameBuf, offset);
    }

    proxy_log(LogCategory::FATAL, "  EAX=%08X EBX=%08X ECX=%08X EDX=%08X",
        ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
    proxy_log(LogCategory::FATAL, "  ESI=%08X EDI=%08X EBP=%08X ESP=%08X",
        ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp);
    proxy_log(LogCategory::FATAL, "  EIP=%08X EFLAGS=%08X",
        ctx->Eip, ctx->EFlags);

    LogCallStack(ctx);

    return EXCEPTION_CONTINUE_SEARCH;
}

static void CheckRequiredDataFiles()
{
    const wchar_t* requiredFiles[] = {
        L"data\\script.src",
        L"data\\TEXT.DAT"
    };

    for (const wchar_t* filePath : requiredFiles)
    {
        if (GetFileAttributesW(filePath) == INVALID_FILE_ATTRIBUTES)
        {
            std::wstringstream ss;
            ss << L"Required data file not found: " << filePath << L"\n\n";
            ss << L"Please ensure you have run VNTextPatch to insert the translated script, or disable enableFontSubstitution in " << RUNTIME_CONFIG_FILENAME;
            ShowErrorAndExit(ss.str());
        }
    }
}

void Initialize();

__declspec(naked) void EntryPointHook()
{
    __asm
    {
        call Initialize
        jmp OriginalEntryPoint
    }
}

void Initialize()
{
    if (OriginalEntryPoint != nullptr)
        DetourDetach(&OriginalEntryPoint, &EntryPointHook);

    // Uncomment for games that only work in a Japanese locale
    // (and include LoaderDll.dll and LocaleEmulator.dll from https://github.com/xupefei/Locale-Emulator/releases)
    /*
    if (GetACP() != 932)
    {
        if (LocaleEmulator::Relaunch())
            ExitProcess(0);
    }
    //*/

    SetCurrentDirectoryW(Path::GetModuleFolderPath(nullptr).c_str());
    RuntimeConfig::Load();
    AddVectoredExceptionHandler(0, VectoredCrashHandler);

    proxy_log(LogCategory::INIT, "VNTextProxy built: " __DATE__ " " __TIME__);
    LogDirectoryListing("Current directory (.\\*)", L".\\*");
    LogDirectoryListing("data\\*", L"data\\*");
    LogDirectoryListing("dll\\*", L"dll\\*");
    LogDirectoryListing("save\\*", L"save\\*");

    CompilerHelper::Init();
    Win32AToWAdapter::Init();
//    SjisTunnelEncoding::PatchGameLookupTable();
//    D2DProportionalizer::Init();

    if (RuntimeConfig::EnableFontSubstitution()) {
        CheckRequiredDataFiles();
        GdiProportionalizer::Init();
        if (!PALGrabCurrentText::Install())
            proxy_log(LogCategory::HOOKS, "WARNING: PALGrabCurrentText::Install failed - text grab will be unavailable");
    }

    EnginePatches::Init();

    if (RuntimeConfig::PillarboxedFullscreen()) {
        if (RuntimeConfig::DirectX11Upscaling())
            DX11Hooks::Install();
        else
            DX9Hooks::Install();
        DirectShowVideoScale::Install();
    }
    else {
        PALVideoFix::Install();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        Proxy::Init(hModule);

#if _DEBUG
        Initialize();
#else
        OriginalEntryPoint = DetourGetEntryPoint(nullptr);
        DetourTransactionBegin();
        DetourAttach(&OriginalEntryPoint, EntryPointHook);
        DetourTransactionCommit();
#endif
        break;
    	
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
