#include "Platform/CrashReport.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#endif

namespace Atom::CrashReport
{
    std::string ExceptionName(std::uint32_t code)
    {
        switch (code)
        {
        case 0xC0000005u: return "access violation";
        case 0xC00000FDu: return "stack overflow";
        case 0xC0000094u: return "integer division by zero";
        case 0xC000001Du: return "illegal instruction";
        case 0xC0000409u: return "stack buffer overrun (security check)";
        case 0xC0000374u: return "heap corruption";
        case 0x80000003u: return "breakpoint";
        case 0xE06D7363u: return "unhandled C++ exception";
        default: break;
        }
        char text[32];
        std::snprintf(text, sizeof(text), "exception 0x%08X", code);
        return text;
    }

    std::string FileStem(int year, int month, int day, int hour, int minute, int second)
    {
        char stem[48];
        std::snprintf(stem, sizeof(stem), "crash-%04d%02d%02d-%02d%02d%02d", year, month, day, hour, minute, second);
        return stem;
    }

#ifdef _WIN32
    namespace
    {
        // Fixed at Install: the handler must not allocate much or rely on
        // the heap being healthy.
        wchar_t g_folder[MAX_PATH]{};
        char g_app[64]{};
        char g_version[32]{};
        bool g_installed = false;

        LONG WINAPI OnCrash(EXCEPTION_POINTERS* exception)
        {
            static volatile LONG once = 0;
            if (InterlockedExchange(&once, 1) != 0)
            {
                return EXCEPTION_CONTINUE_SEARCH; // a crash while reporting a crash
            }
            SYSTEMTIME now{};
            GetLocalTime(&now);
            const std::string stem = FileStem(now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
            wchar_t dumpPath[MAX_PATH]{};
            wchar_t textPath[MAX_PATH]{};
            swprintf_s(dumpPath, L"%s\\%hs.dmp", g_folder, stem.c_str());
            swprintf_s(textPath, L"%s\\%hs.txt", g_folder, stem.c_str());

            // The minidump: every thread, its stack, and the memory its
            // registers point at - enough to see where and usually why.
            bool dumped = false;
            HANDLE file = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                MINIDUMP_EXCEPTION_INFORMATION info{ GetCurrentThreadId(), exception, FALSE };
                const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo
                                                             | MiniDumpWithUnloadedModules);
                dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr);
                CloseHandle(file);
            }

            // The text report: what, where (module + offset: no paths).
            const DWORD code = exception->ExceptionRecord->ExceptionCode;
            const void* address = exception->ExceptionRecord->ExceptionAddress;
            char module[MAX_PATH] = "unknown";
            std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(address);
            HMODULE handle = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCSTR>(address), &handle))
            {
                char full[MAX_PATH]{};
                GetModuleFileNameA(handle, full, MAX_PATH);
                const char* name = std::strrchr(full, '\\');
                std::snprintf(module, sizeof(module), "%s", name ? name + 1 : full);
                offset -= reinterpret_cast<std::uintptr_t>(handle);
            }
            if (FILE* text = _wfopen(textPath, L"w"))
            {
                std::fprintf(text,
                             "crash: %s %s\n"
                             "time: %04d-%02d-%02d %02d:%02d:%02d\n"
                             "exception: %s (0x%08lX)\n"
                             "where: %s+0x%llX\n"
                             "minidump: %s\n",
                             g_app, g_version, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
                             ExceptionName(code).c_str(), code, module, static_cast<unsigned long long>(offset),
                             dumped ? (stem + ".dmp").c_str() : "not written");
                std::fclose(text);
            }
            std::fprintf(stderr, "%s crashed (%s at %s+0x%llX); report: %s.txt%s\n", g_app, ExceptionName(code).c_str(),
                         module, static_cast<unsigned long long>(offset), stem.c_str(), dumped ? ", minidump .dmp" : "");
            std::fflush(stderr);
            return EXCEPTION_EXECUTE_HANDLER; // the process ends
        }
    }

    void Install(const std::string& folder, const std::string& app)
    {
        std::error_code error;
        std::filesystem::create_directories(std::filesystem::u8path(folder), error);
        const std::wstring wide = std::filesystem::u8path(folder).wstring();
        wcsncpy_s(g_folder, wide.c_str(), _TRUNCATE);
        std::snprintf(g_app, sizeof(g_app), "%s", app.c_str());
        std::snprintf(g_version, sizeof(g_version), "%s", ATOM_VERSION);
        SetUnhandledExceptionFilter(&OnCrash);
        g_installed = true;
    }

    bool IsInstalled() { return g_installed; }
#else
    void Install(const std::string&, const std::string&) {}
    bool IsInstalled() { return false; }
#endif
}
