#include "MyJVCrashHandler.h"

#include "MyJVLog.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dbghelp.h>
 #if JUCE_MSVC
  #pragma comment(lib, "DbgHelp.lib")
 #endif
#endif

namespace
{
bool installed = false;
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

void writeMiniDump (EXCEPTION_POINTERS* exceptionInfo, const juce::File& file)
{
    const auto handle = CreateFileW (file.getFullPathName().toWideCharPointer(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (handle == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION info {};
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exceptionInfo;
    info.ClientPointers = FALSE;

    MiniDumpWriteDump (GetCurrentProcess(), GetCurrentProcessId(), handle, MiniDumpNormal, &info, nullptr, nullptr);

    CloseHandle (handle);
}

LONG WINAPI handleUnhandledException (EXCEPTION_POINTERS* exceptionInfo)
{
    MyJVCrashHandler::CrashInfo crashInfo;
    crashInfo.timestamp = juce::Time::getCurrentTime().toString (true, true);
    crashInfo.version = installedVersion;
    crashInfo.exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    crashInfo.exceptionAddress = reinterpret_cast<juce::uint64> (exceptionInfo->ExceptionRecord->ExceptionAddress);

    const auto directory = MyJVLog::defaultLogDirectory();
    const auto report = MyJVCrashHandler::writeCrashReport (crashInfo, directory);

    writeMiniDump (exceptionInfo, report.withFileExtension (".dmp"));

    if (previousFilter != nullptr)
        return previousFilter (exceptionInfo);

    return EXCEPTION_CONTINUE_SEARCH;
}
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
    if (installed)
        return;

    installed = true;
    installedVersion = version;

#if JUCE_WINDOWS
    previousFilter = SetUnhandledExceptionFilter (handleUnhandledException);
#endif
}
}
