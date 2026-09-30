#include "pc_fatal_log.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

namespace {

void note(const char* fmt, unsigned long a = 0, void* b = nullptr)
{
    std::fprintf(stderr, "[PC Port Fatal] ");
    std::fprintf(stderr, fmt, a, b);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

void orderlyExit()
{
    std::fprintf(stderr, "[PC Port] orderly process exit (CRT atexit chain ran)\n");
    std::fflush(stderr);
}

void terminateHandler()
{
    if (std::exception_ptr active = std::current_exception()) {
        try {
            std::rethrow_exception(active);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[PC Port Fatal] std::terminate after uncaught exception: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "[PC Port Fatal] std::terminate after uncaught non-standard exception\n");
        }
    } else {
        std::fprintf(stderr, "[PC Port Fatal] std::terminate with no active exception\n");
    }
    std::fflush(stderr);
    std::abort();
}

void abortSignal(int)
{
    // abort() lands here (OSPanic, the many std::abort() validation guards).
    // The guard that called it normally printed its own reason just before.
    std::fprintf(stderr, "[PC Port Fatal] abort() called (SIGABRT); see the lines above for the failing guard\n");
    std::fflush(stderr);
    std::signal(SIGABRT, SIG_DFL);
    std::abort();
}

#ifdef _WIN32
const char* exceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case 0xC0000409: return "fast fail / stack buffer overrun";
    default: return "exception";
    }
}

LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* info)
{
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    void* at = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
    std::fprintf(stderr, "[PC Port Fatal] unhandled %s code=0x%08lx at %p", exceptionName(code),
                 static_cast<unsigned long>(code), at);
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
        std::fprintf(stderr, " (%s address %p)",
                     info->ExceptionRecord->ExceptionInformation[0] == 0 ? "read of"
                     : info->ExceptionRecord->ExceptionInformation[0] == 1 ? "write to" : "execute at",
                     reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH; // let Windows Error Reporting record it too
}

BOOL WINAPI consoleCtrl(DWORD type)
{
    const char* name = type == CTRL_C_EVENT ? "Ctrl+C" : type == CTRL_BREAK_EVENT ? "Ctrl+Break"
                     : type == CTRL_CLOSE_EVENT ? "console window closed" : type == CTRL_LOGOFF_EVENT ? "user logoff"
                     : type == CTRL_SHUTDOWN_EVENT ? "system shutdown" : "unknown";
    std::fprintf(stderr, "[PC Port Fatal] console control event %lu (%s); the launcher console asked this process to stop\n",
                 static_cast<unsigned long>(type), name);
    std::fflush(stderr);
    return FALSE; // keep the default handling (exit)
}

void invalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t)
{
    std::fprintf(stderr, "[PC Port Fatal] invalid CRT parameter (e.g. bad printf/file argument)\n");
    std::fflush(stderr);
    std::abort();
}
#endif

} // namespace

void pc_fatal_log_install()
{
    std::atexit(orderlyExit);
    std::set_terminate(terminateHandler);
    std::signal(SIGABRT, abortSignal);
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandledFilter);
    SetConsoleCtrlHandler(consoleCtrl, TRUE);
    _set_invalid_parameter_handler(invalidParameter);
#endif
}
