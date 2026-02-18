#include "pch.h"
#include "Util/Logger.h"

using namespace std;

void ImportHooker::Hook(const map<string, void*>& replacementFuncs)
{
    Init();

    for (auto pair : replacementFuncs)
    {
        ReplacementFuncs.insert(pair);
    }

    HMODULE hExe = GetModuleHandle(nullptr);
    proxy_log(LogCategory::HOOKS, "ImportHooker::Hook: patching main EXE IAT (%zu funcs)", replacementFuncs.size());
    DetourEnumerateImportsEx(hExe, (void*)&replacementFuncs, nullptr, PatchGameImport);
}

void ImportHooker::ApplyToModule(HMODULE hModule)
{
    char moduleName[MAX_PATH] = "<unknown>";
    GetModuleFileNameA(hModule, moduleName, sizeof(moduleName));
    proxy_log(LogCategory::HOOKS, "ImportHooker::ApplyToModule: patching IAT of %s", moduleName);
    DetourEnumerateImportsEx(hModule, (void*)&ReplacementFuncs, nullptr, PatchGameImport);
}

void ImportHooker::Init()
{
    if (Initialized)
        return;

    Initialized = true;
    Hook(
        {
            { "GetProcAddress", GetProcAddressHook }
        }
    );
}

BOOL ImportHooker::PatchGameImport(void* pContext, DWORD nOrdinal, LPCSTR pszFunc, void** ppvFunc)
{
    if (pszFunc == nullptr || ppvFunc == nullptr)
        return true;

    map<string, void*>* pReplacementFuncs = (map<string, void*>*)pContext;
    auto it = pReplacementFuncs->find(pszFunc);
    if (it != pReplacementFuncs->end())
    {
        void* oldValue = *ppvFunc;
        proxy_log(LogCategory::HOOKS, "ImportHooker::PatchGameImport: patching '%s' at IAT 0x%p: old=0x%p -> new=0x%p", pszFunc, ppvFunc, oldValue, it->second);
        MemoryUnprotector unprotect(ppvFunc, 4);
        *ppvFunc = it->second;
        void* verify = *ppvFunc;
        if (verify != it->second)
            proxy_log(LogCategory::HOOKS, "ImportHooker::PatchGameImport: WRITE FAILED for '%s': expected 0x%p, got 0x%p", pszFunc, it->second, verify);
    }

    return true;
}

FARPROC ImportHooker::GetProcAddressHook(HMODULE hModule, LPCSTR lpProcName)
{
    auto it = ReplacementFuncs.find(lpProcName);
    if (it != ReplacementFuncs.end())
        return (FARPROC)it->second;

    return GetProcAddress(hModule, lpProcName);
}
