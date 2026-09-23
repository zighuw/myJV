#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "Plugin/MyJVCrashHandler.h"
#include "Plugin/MyJVLog.h"

namespace
{
using namespace juce;

File makeTestDirectory()
{
    auto directory = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVDiagnosticsTests");
    directory.deleteRecursively();
    return directory;
}
}

TEST_CASE ("log initialisation creates and writes the log file")
{
    const auto directory = makeTestDirectory();

    MyJVLog::initialise (directory);
    MyJVLog::logInfo ("hello diagnostics");
    MyJVLog::logWarning ("warning diagnostics");
    MyJVLog::logError ("error diagnostics");
    MyJVLog::shutdown();

    const auto logFile = directory.getChildFile ("myJV.log");
    REQUIRE (logFile.existsAsFile());

    const auto text = logFile.loadFileAsString();
    REQUIRE (text.contains ("hello diagnostics"));
    REQUIRE (text.contains ("[WARN] warning diagnostics"));
    REQUIRE (text.contains ("[ERROR] error diagnostics"));
}

TEST_CASE ("log initialisation is reference counted")
{
    const auto directory = makeTestDirectory();

    MyJVLog::initialise (directory);
    MyJVLog::initialise (directory);
    MyJVLog::shutdown();

    REQUIRE (Logger::getCurrentLogger() != nullptr);

    MyJVLog::logInfo ("still logging");
    MyJVLog::shutdown();

    REQUIRE (Logger::getCurrentLogger() == nullptr);
}

TEST_CASE ("crash report writer creates a report with the exception details")
{
    const auto directory = makeTestDirectory();

    MyJVCrashHandler::CrashInfo info;
    info.timestamp = "2026-09-22 12:34:56";
    info.version = "0.1.0";
    info.exceptionCode = 0xC0000005u;
    info.exceptionAddress = 0x00007FF6AABBCCDDull;

    const auto report = MyJVCrashHandler::writeCrashReport (info, directory);

    REQUIRE (report.existsAsFile());

    const auto text = report.loadFileAsString();
    REQUIRE (text.contains ("2026-09-22 12:34:56"));
    REQUIRE (text.contains ("0.1.0"));
    REQUIRE (text.contains ("C0000005"));
    REQUIRE (text.contains ("7FF6AABBCCDD"));
    REQUIRE (text.contains ("crash-2026-09-22123456.dmp"));
    REQUIRE (directory.getChildFile ("crash-2026-09-22123456.txt").existsAsFile());
}
