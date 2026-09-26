#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "IO/LibraryIndex.h"
#include "Model/SampleLibrary.h"
#include "Model/ZoneSet.h"

#include <memory>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace juce;

const std::string kAbcHash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
const std::string kFffffHash = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

File makeLibraryDirectory()
{
    auto directory = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVLibraryTests");
    directory.deleteRecursively (false);
    directory.createDirectory();
    return directory;
}

File writeFile (const File& directory, const String& relativePath, const String& content)
{
    auto file = directory.getChildFile (relativePath);
    file.getParentDirectory().createDirectory();
    REQUIRE (file.replaceWithText (content));
    return file;
}

const LibraryEntry* findEntry (const SampleLibrary& library, const std::string& relativePath)
{
    for (const auto& entry : library.getEntries())
        if (entry.relativePath == relativePath)
            return &entry;

    return nullptr;
}

LibraryEntry makeEntry (const std::string& relativePath, const std::string& fileHash)
{
    LibraryEntry entry;
    entry.relativePath = relativePath;
    entry.fileHash = fileHash;
    return entry;
}

bool createDirectoryLink (const File& link, const File& target)
{
#if JUCE_WINDOWS
    const auto command = "cmd /c mklink /J \"" + link.getFullPathName() + "\" \""
                       + target.getFullPathName() + "\"";
    return std::system (command.toRawUTF8()) == 0 && link.isDirectory();
#else
    return target.createSymbolicLink (link, false) && link.isDirectory();
#endif
}
}

TEST_CASE ("library index round-trips entries through json")
{
    const auto directory = makeLibraryDirectory();
    const auto indexFile = directory.getChildFile ("library.json");

    auto first = makeEntry ("Piano/Piano_C2.wav", kAbcHash);
    first.rootKey = 36;
    first.sourceSampleRate = 44100.0;
    first.lengthSamples = 123456;
    first.thumbnailPath = "Piano/thumbs/Piano_C2.png";
    first.loop.start = 120;
    first.loop.end = 480;
    first.loop.crossfadeSamples = 32;

    auto second = makeEntry ("Strings/Arco.flac", kFffffHash);
    second.rootKey = 72;
    second.sourceSampleRate = 48000.0;

    REQUIRE (LibraryIndex::save (indexFile, { first, second }));
    REQUIRE (indexFile.existsAsFile());

    const auto json = indexFile.loadFileAsString();
    REQUIRE (json.contains ("schemaVersion"));
    REQUIRE (json.contains ("Piano/Piano_C2.wav"));

    std::vector<LibraryEntry> loaded;
    REQUIRE (LibraryIndex::load (indexFile, loaded));
    REQUIRE (loaded.size() == 2);

    REQUIRE (loaded[0].relativePath == first.relativePath);
    CHECK (loaded[0].fileHash == first.fileHash);
    CHECK (loaded[0].rootKey == 36);
    CHECK (loaded[0].sourceSampleRate == 44100.0);
    CHECK (loaded[0].lengthSamples == 123456);
    CHECK (loaded[0].thumbnailPath == "Piano/thumbs/Piano_C2.png");
    CHECK (loaded[0].loop.start == 120);
    CHECK (loaded[0].loop.end == 480);
    CHECK (loaded[0].loop.crossfadeSamples == 32);

    REQUIRE (loaded[1].relativePath == second.relativePath);
    CHECK (loaded[1].fileHash == second.fileHash);
    CHECK (loaded[1].rootKey == 72);
    CHECK (loaded[1].sourceSampleRate == 48000.0);
    CHECK (loaded[1].lengthSamples == 0);
    CHECK (loaded[1].thumbnailPath.empty());
    CHECK (loaded[1].loop.start == 0);
    CHECK (loaded[1].loop.end == 0);
    CHECK (loaded[1].loop.crossfadeSamples == 0);
}

TEST_CASE ("library index save creates missing parent directories")
{
    const auto directory = makeLibraryDirectory();
    const auto indexFile = directory.getChildFile ("nested/library.json");

    REQUIRE (LibraryIndex::save (indexFile, { makeEntry ("A.wav", kAbcHash) }));
    REQUIRE (indexFile.existsAsFile());
}

TEST_CASE ("library index load tolerates missing and corrupt files")
{
    const auto directory = makeLibraryDirectory();

    std::vector<LibraryEntry> entries { makeEntry ("stale.wav", kAbcHash) };
    CHECK_FALSE (LibraryIndex::load (directory.getChildFile ("missing.json"), entries));
    CHECK (entries.empty());

    const auto corrupt = writeFile (directory, "corrupt.json", "this is not json {{{");
    entries.push_back (makeEntry ("stale.wav", kAbcHash));
    CHECK_FALSE (LibraryIndex::load (corrupt, entries));
    CHECK (entries.empty());
}

TEST_CASE ("library index rejects unsupported schema versions")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "future.json", "{\"schemaVersion\": 99, \"samples\": []}");

    std::vector<LibraryEntry> entries;
    CHECK_FALSE (LibraryIndex::load (file, entries));
    CHECK (entries.empty());
}

TEST_CASE ("scan indexes supported audio files recursively")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "Piano/C2.wav", "abc");
    writeFile (directory, "Piano/Sub/Soft.aif", "def");
    writeFile (directory, "Drums/kick.FLAC", "ghi");
    writeFile (directory, "Notes/readme.txt", "text");
    writeFile (directory, "Piano/cover.png", "png");

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto result = library.scanNow();

    REQUIRE (result.succeeded);
    CHECK (result.filesFound == 3);
    CHECK (result.entriesAdded == 3);
    CHECK (result.entriesUpdated == 0);
    CHECK (result.duplicatesSkipped == 0);
    CHECK (result.missingPaths.empty());
    REQUIRE (library.getEntries().size() == 3);

    const auto* c2 = findEntry (library, "Piano/C2.wav");
    REQUIRE (c2 != nullptr);
    CHECK (c2->fileHash == kAbcHash);
    CHECK (c2->rootKey == 60);
    CHECK (c2->sourceSampleRate == 48000.0);
    CHECK (c2->lengthSamples == 0);
    CHECK (c2->thumbnailPath.empty());

    CHECK (findEntry (library, "Piano/Sub/Soft.aif") != nullptr);
    CHECK (findEntry (library, "Drums/kick.FLAC") != nullptr);
}

TEST_CASE ("scan skips duplicate content and keeps the first path")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "B.wav", "duplicate-content");
    writeFile (directory, "A.wav", "duplicate-content");

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto result = library.scanNow();

    REQUIRE (result.succeeded);
    CHECK (result.filesFound == 2);
    CHECK (result.entriesAdded == 1);
    CHECK (result.duplicatesSkipped == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "A.wav");
}

TEST_CASE ("rescanning an unchanged library is idempotent")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "Samples/A.wav", "one");
    writeFile (directory, "Samples/B.wav", "two");

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto first = library.scanNow();
    REQUIRE (first.entriesAdded == 2);

    const auto second = library.scanNow();

    CHECK (second.succeeded);
    CHECK (second.filesFound == 2);
    CHECK (second.entriesAdded == 0);
    CHECK (second.entriesUpdated == 0);
    CHECK (second.duplicatesSkipped == 0);
    CHECK (second.missingPaths.empty());
    CHECK (library.getEntries().size() == 2);
}

TEST_CASE ("changed file content updates the hash and keeps entry metadata")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "A.wav", "original");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 1);

    auto entries = library.getEntries();
    entries[0].rootKey = 48;
    entries[0].loop.start = 10;
    entries[0].loop.end = 20;
    library.setEntries (entries);
    const auto originalHash = library.getEntries()[0].fileHash;

    REQUIRE (file.replaceWithText ("modified"));

    const auto result = library.scanNow();

    CHECK (result.entriesUpdated == 1);
    CHECK (result.entriesAdded == 0);
    CHECK (result.missingPaths.empty());
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].fileHash != originalHash);
    CHECK (library.getEntries()[0].rootKey == 48);
    CHECK (library.getEntries()[0].loop.start == 10);
    CHECK (library.getEntries()[0].loop.end == 20);
}

TEST_CASE ("deleted files are reported missing but stay indexed")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "A.wav", "to-delete");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 1);

    REQUIRE (file.deleteFile());

    const auto result = library.scanNow();

    CHECK (result.succeeded);
    CHECK (result.filesFound == 0);
    CHECK (result.entriesAdded == 0);
    REQUIRE (result.missingPaths.size() == 1);
    CHECK (result.missingPaths[0] == "A.wav");
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "A.wav");
}

TEST_CASE ("new files are added on rescan")
{
    const auto directory = makeLibraryDirectory();

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().filesFound == 0);

    writeFile (directory, "New.wav", "fresh");

    const auto result = library.scanNow();

    CHECK (result.entriesAdded == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "New.wav");
}

TEST_CASE ("a new duplicate of an indexed file is skipped without losing metadata")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "shared-content");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 1);

    auto entries = library.getEntries();
    entries[0].rootKey = 36;
    library.setEntries (entries);

    writeFile (directory, "Z.wav", "shared-content");

    const auto result = library.scanNow();

    CHECK (result.entriesAdded == 0);
    CHECK (result.duplicatesSkipped == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "A.wav");
    CHECK (library.getEntries()[0].rootKey == 36);
}

TEST_CASE ("content changing to collide with another entry drops the later duplicate")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "one");
    const auto b = writeFile (directory, "B.wav", "two");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 2);
    REQUIRE (library.getEntries().size() == 2);

    REQUIRE (b.replaceWithText ("one"));

    const auto result = library.scanNow();

    CHECK (result.duplicatesSkipped == 1);
    CHECK (result.missingPaths.empty());
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "A.wav");
}

TEST_CASE ("sample cache publishes and releases shared samples")
{
    auto sample = std::make_shared<Sample>();
    sample->rootKey = 42;

    SampleLibrary library;
    library.addSample (kAbcHash, sample);

    CHECK (library.findSample (kAbcHash).get() == sample.get());

    Zone zone;
    zone.sample = library.findSample (kAbcHash);

    library.removeSample (kAbcHash);

    CHECK (library.findSample (kAbcHash) == nullptr);
    REQUIRE (zone.sample != nullptr);
    CHECK (zone.sample->rootKey == 42);
}

TEST_CASE ("background scan publishes the index on the message thread")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "one");
    writeFile (directory, "B.flac", "two");

    SampleLibrary library;
    library.setRootDirectory (directory);

    int callbacks = 0;
    ScanResult delivered;
    std::thread::id callbackThread;
    library.onScanComplete = [&] (const ScanResult& result)
    {
        ++callbacks;
        delivered = result;
        callbackThread = std::this_thread::get_id();
    };

    library.startScan();
    library.startScan();   // ignored: a scan is already in flight

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 10000.0;

    while (callbacks == 0 && juce::Time::getMillisecondCounterHiRes() < deadline)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (5);

    library.waitForScanToFinish();

    CHECK (callbacks == 1);
    CHECK (callbackThread == std::this_thread::get_id());
    CHECK_FALSE (library.isScanning());
    CHECK (delivered.succeeded);
    CHECK (delivered.entriesAdded == 2);
    REQUIRE (library.getEntries().size() == 2);
    CHECK (findEntry (library, "A.wav") != nullptr);
    CHECK (findEntry (library, "B.flac") != nullptr);
}

TEST_CASE ("missing entries do not suppress a present duplicate file")
{
    const auto directory = makeLibraryDirectory();
    const auto a = writeFile (directory, "A.wav", "shared-content");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 1);

    REQUIRE (a.deleteFile());
    writeFile (directory, "B.wav", "shared-content");

    const auto result = library.scanNow();

    CHECK (result.entriesAdded == 1);
    CHECK (result.duplicatesSkipped == 0);
    REQUIRE (result.missingPaths.size() == 1);
    CHECK (result.missingPaths[0] == "A.wav");
    REQUIRE (library.getEntries().size() == 2);
    CHECK (findEntry (library, "A.wav") != nullptr);
    CHECK (findEntry (library, "B.wav") != nullptr);
}

TEST_CASE ("scan does not follow directory links")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "abc");

    const auto link = directory.getChildFile ("loop");

    if (! createDirectoryLink (link, directory))
        return;   // platform without link-creation privileges (e.g. Windows without admin)

    REQUIRE (link.getChildFile ("A.wav").existsAsFile());

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto result = library.scanNow();

    REQUIRE (result.succeeded);
    CHECK (result.filesFound == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].relativePath == "A.wav");
}

TEST_CASE ("library index tolerates optional fields and coerces wrong types")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "sparse.json",
        "{\"schemaVersion\": 1, \"extra\": 42, \"samples\": ["
        "{\"path\": \"A.wav\", \"hash\": \"abc\"},"
        "{\"path\": \"B.wav\", \"hash\": \"def\", \"rootKey\": {}, \"sourceSampleRate\": \"x\", \"loop\": null},"
        "{\"path\": \"C.wav\", \"hash\": \"ghi\", \"loop\": 7}"
        "]}");

    std::vector<LibraryEntry> entries;
    REQUIRE (LibraryIndex::load (file, entries));
    REQUIRE (entries.size() == 3);

    CHECK (entries[0].rootKey == 60);
    CHECK (entries[0].sourceSampleRate == 48000.0);
    CHECK (entries[0].loop.start == 0);

    CHECK (entries[1].rootKey == 60);
    CHECK (entries[1].sourceSampleRate == 48000.0);
    CHECK (entries[1].loop.end == 0);

    CHECK (entries[2].rootKey == 60);
    CHECK (entries[2].loop.crossfadeSamples == 0);
}

TEST_CASE ("library index skips unsafe, unhashed and malformed samples")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "unsafe.json",
        "{\"schemaVersion\": 1, \"samples\": ["
        "{\"path\": \"../evil.wav\", \"hash\": \"abc\"},"
        "{\"path\": \"sub/../evil.wav\", \"hash\": \"def\"},"
        "{\"path\": \"C.wav\", \"hash\": \"\"},"
        "{\"path\": \"D.wav\", \"hash\": \"ghi\"}"
        "]}");

    std::vector<LibraryEntry> entries;
    REQUIRE (LibraryIndex::load (file, entries));
    REQUIRE (entries.size() == 1);
    CHECK (entries[0].relativePath == "D.wav");

    const auto notArray = writeFile (directory, "not-array.json",
        "{\"schemaVersion\": 1, \"samples\": 7}");

    entries.push_back (makeEntry ("stale.wav", kAbcHash));
    REQUIRE (LibraryIndex::load (notArray, entries));
    CHECK (entries.empty());
}

TEST_CASE ("library index rejects non-integral schema versions")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "fractional.json",
        "{\"schemaVersion\": 1.5, \"samples\": []}");

    std::vector<LibraryEntry> entries;
    CHECK_FALSE (LibraryIndex::load (file, entries));
    CHECK (entries.empty());
}
