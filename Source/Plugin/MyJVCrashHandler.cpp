#include "MyJVCrashHandler.h"

#include "MyJVLog.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dbghelp.h>
 #if JUCE_MSVC
  #pragma comment(lib, "DbgHelp.lib")
 #endif
#endif

#include <atomic>

namespace
{
std::atomic<bool> installed { false };
juce::String installedVersion;

juce::String sanitiseForFileName (const juce::String& text)
{
    return text.retainCharacters ("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_");
}

juce::String toHex (juce::uint64 value, int digits)
{
    return juce::String::toHexString (static_cast<juce::int64> (value)).paddedLeft ('0', digits).toUpperCase();
}

juce::String buildReportText (const MyJVCrashHandler::CrashInfo& info, const juce::String& dumpFileName)
{
    juce::String text;

    text << "myJV crash report\n"
         << "timestamp: " << info.timestamp << "\n"
         << "version: " << info.version << "\n"
         << "exception code: 0x" << toHex (info.exceptionCode, 8) << "\n"
         << "exception address: 0x" << toHex (info.exceptionAddress, 16) << "\n"
         << "minidump: " << dumpFileName << "\n";

    return text;
}

#if JUCE_WINDOWS
LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
volatile LONG handlerRunning = 0;

bool writeMiniDump (EXCEPTION_POINTERS* exceptionInfo, const juce::File& file)
{
    const auto handle = CreateFileW (file.getFullPathName().toWideCharPointer(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE)
        return false;

    MINIDUMP_EXCEPTION_INFORMATION info {};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exceptionInfo;
    info.ClientPointers = FALSE;

    const auto success = MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), handle,
                                            MiniDumpNormal, &info, nullptr, nullptr) != FALSE;

    CloseHandle (handle);

    return success;
}

LONG WINAPI handleUnhandledException (EXCEPTION_POINTERS* exceptionInfo)
{
    if (exceptionInfo == nullptr || exceptionInfo->ExceptionRecord == nullptr)
        return previousFilter != nullptr ? previousFilter (exceptionInfo) : EXCEPTION_CONTINUE_SEARCH;

    if (InterlockedCompareExchange (&handlerRunning, 1, 0) != 0)
        return EXCEPTION_CONTINUE_SEARCH;

    MyJVCrashHandler::CrashInfo crashInfo;
    crashInfo.timestamp = juce::Time::getCurrentTime().toString (true, true);
    crashInfo.version = installedVersion;
    crashInfo.exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    crashInfo.exceptionAddress = reinterpret_cast<juce::uint64> (exceptionInfo->ExceptionRecord->ExceptionAddress);

    const auto directory = MyJVLog::defaultLogDirectory();
    const auto report = MyJVCrashHandler::writeCrashReport (crashInfo, directory);

    if (! writeMiniDump (exceptionInfo, report.withFileExtension (".dmp")))
        report.appendText ("minidump write failed\n");

    if (previousFilter != nullptr)
        return previousFilter (exceptionInfo);

    return EXCEPTION_CONTINUE_SEARCH;
}

struct UnhandledExceptionFilterScope
{
    ~UnhandledExceptionFilterScope()
    {
        if (installed.load (std::memory_order_acquire))
            SetUnhandledExceptionFilter (previousFilter);
    }
};

UnhandledExceptionFilterScope filterScope;
#endif
}

namespace MyJVCrashHandler
{
juce::File writeCrashReport (const CrashInfo& info, const juce::File& directory)
{
    directory.createDirectory();

    const auto report = directory.getChildFile ("crash-" + sanitiseForFileName (info.timestamp) + ".txt");
    const auto dump = report.withFileExtension (".dmp");

    report.replaceWithText (buildReportText (info, dump.getFileName()));

    return report;
}

void install (const juce::String& version)
{
    bool expected = false;

    if (! installed.compare_exchange_strong (expected, true, std::memory_order_acq_rel))
        return;

    installedVersion = version;

#if JUCE_WINDOWS
    previousFilter = SetUnhandledExceptionFilter (handleUnhandledException);
#endif
}
}
