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

File writeFile (const File& directory, const String& path, const String& content)
{
    auto file = directory.getChildFile (path);
    file.getParentDirectory().createDirectory();
    REQUIRE (file.replaceWithText (content));
    return file;
}

const LibraryEntry* findEntry (const SampleLibrary& library, const std::string& path)
{
    for (const auto& entry : library.getEntries())
        if (entry.path == path)
            return &entry;

    return nullptr;
}

LibraryEntry makeEntry (const std::string& path, const std::string& fileHash)
{
    LibraryEntry entry;
    entry.path = path;
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

// A 64-character lowercase hash whose digits are all `digit`, for JSON fixtures.
String hexHash (char digit)
{
    return String::repeatedString (String::charToString ((juce_wchar) digit), 64);
}

File writeBigFile (const File& directory, const String& path, int sizeBytes)
{
    auto file = directory.getChildFile (path);
    file.deleteFile();

    std::unique_ptr<FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
    {
        REQUIRE (false);
        return file;
    }

    const std::vector<char> chunk (64 * 1024, 0);
    int written = 0;

    while (written < sizeBytes)
    {
        const auto numBytes = jmin ((int) chunk.size(), sizeBytes - written);

        if (! stream->write (chunk.data(), (std::size_t) numBytes))
        {
            REQUIRE (false);
            break;
        }

        written += numBytes;
    }

    stream->flush();
    stream.reset();
    return file;
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
    first.thumbnailPath = "thumbnails/" + kAbcHash + ".png";
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

    REQUIRE (loaded[0].path == first.path);
    CHECK (loaded[0].fileHash == first.fileHash);
    CHECK (loaded[0].rootKey == 36);
    CHECK (loaded[0].sourceSampleRate == 44100.0);
    CHECK (loaded[0].lengthSamples == 123456);
    CHECK (loaded[0].thumbnailPath == first.thumbnailPath);
    CHECK (loaded[0].loop.start == 120);
    CHECK (loaded[0].loop.end == 480);
    CHECK (loaded[0].loop.crossfadeSamples == 32);
    CHECK_FALSE (first.external);
    CHECK_FALSE (loaded[0].external);

    REQUIRE (loaded[1].path == second.path);
    CHECK (loaded[1].fileHash == second.fileHash);
    CHECK (loaded[1].rootKey == 72);
    CHECK (loaded[1].sourceSampleRate == 48000.0);
    CHECK (loaded[1].lengthSamples == 0);
    CHECK (loaded[1].thumbnailPath.empty());
    CHECK (loaded[1].loop.start == 0);
    CHECK (loaded[1].loop.end == 0);
    CHECK (loaded[1].loop.crossfadeSamples == 0);
}

TEST_CASE ("library index round-trips external entries")
{
    const auto directory = makeLibraryDirectory();
    const auto indexFile = directory.getChildFile ("library.json");
    const auto externalFile = writeFile (directory, "Outside/Sample.wav", "abc");
    const auto absolutePath = externalFile.getFullPathName().replaceCharacter ('\\', '/').toStdString();

    auto entry = makeEntry (absolutePath, kAbcHash);
    entry.external = true;

    REQUIRE (LibraryIndex::save (indexFile, { entry }));

    const auto json = indexFile.loadFileAsString();
    REQUIRE (json.contains ("external"));

    std::vector<LibraryEntry> loaded;
    REQUIRE (LibraryIndex::load (indexFile, loaded));
    REQUIRE (loaded.size() == 1);
    CHECK (loaded[0].external);
    CHECK (loaded[0].path == absolutePath);
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
    CHECK (library.getEntries()[0].path == "A.wav");
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
    CHECK (library.getEntries()[0].path == "A.wav");
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
    CHECK (library.getEntries()[0].path == "New.wav");
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
    CHECK (library.getEntries()[0].path == "A.wav");
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
    CHECK (library.getEntries()[0].path == "A.wav");
}

TEST_CASE ("scan reports existing entries dropped by deduplication")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "one");
    const auto b = writeFile (directory, "B.wav", "two");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().entriesAdded == 2);

    REQUIRE (findEntry (library, "A.wav") != nullptr);
    const auto firstHash = findEntry (library, "A.wav")->fileHash;

    // B.wav now has the same content as A.wav, so the later existing entry is
    // dropped from the index; that must be reported instead of silently
    // shrinking the library (code-review I-6).
    REQUIRE (b.replaceWithText ("one"));

    const auto result = library.scanNow();

    CHECK (result.duplicatesSkipped == 1);
    CHECK (result.entriesDropped == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].path == "A.wav");
    CHECK (library.getEntries()[0].fileHash == firstHash);

    // An unchanged rescan drops nothing more: the entry is already gone and the
    // file is now a plain on-disk duplicate, skipped on every scan.
    const auto clean = library.scanNow();
    CHECK (clean.entriesDropped == 0);
    CHECK (clean.entriesAdded == 0);
    CHECK (clean.entriesUpdated == 0);
    CHECK (clean.duplicatesSkipped == 1);
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

TEST_CASE ("cancelling a scan leaves the index and hashes untouched")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "one");
    writeFile (directory, "B.wav", "two");

    SampleLibrary library;
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().succeeded);

    const auto before = library.getEntries();
    REQUIRE (before.size() == 2);

    // A 16 MiB file keeps the scan in flight: hashing it takes tens of
    // milliseconds while startScan() + cancelScan() is a sub-millisecond round
    // trip, so the cancellation always lands mid-scan. No sleeping or polling
    // is involved, and the assertions below hold either way (code-review I-7).
    writeBigFile (directory, "Big.wav", 16 * 1024 * 1024);

    library.startScan();
    REQUIRE (library.cancelScan());
    library.waitForScanToFinish();

    CHECK_FALSE (library.isScanning());

    const auto after = library.getEntries();
    REQUIRE (after.size() == before.size());

    for (std::size_t i = 0; i < before.size(); ++i)
    {
        CHECK (after[i].path == before[i].path);
        CHECK (after[i].fileHash == before[i].fileHash);
    }

    // Nothing of the aborted scan leaked into the index.
    CHECK (findEntry (library, "Big.wav") == nullptr);

    // A rescan after the cancellation matches a library that never cancelled.
    REQUIRE (library.scanNow().succeeded);

    SampleLibrary reference;
    reference.setRootDirectory (directory);
    REQUIRE (reference.scanNow().succeeded);

    REQUIRE (library.getEntries().size() == reference.getEntries().size());

    for (std::size_t i = 0; i < reference.getEntries().size(); ++i)
    {
        CHECK (library.getEntries()[i].path == reference.getEntries()[i].path);
        CHECK (library.getEntries()[i].fileHash == reference.getEntries()[i].fileHash);
    }
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
        SKIP ("需要目录链接权限（Windows 非管理员/无特权平台）");   // code-review I-5: was a silent `return`

    REQUIRE (link.getChildFile ("A.wav").existsAsFile());

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto result = library.scanNow();

    REQUIRE (result.succeeded);
    CHECK (result.filesFound == 1);
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].path == "A.wav");
}

TEST_CASE ("library index tolerates optional fields and coerces wrong types")
{
    const auto directory = makeLibraryDirectory();
    const auto file = writeFile (directory, "sparse.json",
        "{\"schemaVersion\": 1, \"extra\": 42, \"samples\": ["
        "{\"path\": \"A.wav\", \"hash\": \"" + hexHash ('a') + "\"},"
        "{\"path\": \"B.wav\", \"hash\": \"" + hexHash ('b') + "\", \"rootKey\": {}, \"sourceSampleRate\": \"x\", \"loop\": null},"
        "{\"path\": \"C.wav\", \"hash\": \"" + hexHash ('c') + "\", \"loop\": 7}"
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
    const auto absolute = writeFile (directory, "Outside/Real.wav", "abc")
                              .getFullPathName().replaceCharacter ('\\', '/');
    const auto dotted = File::getSpecialLocation (File::tempDirectory).getFullPathName().replaceCharacter ('\\', '/')
                        + "/../evil.wav";

    const auto json = juce::String ("{\"schemaVersion\": 1, \"samples\": [")
                      + "{\"path\": \"../evil.wav\", \"hash\": \"" + hexHash ('a') + "\"},"
                      + "{\"path\": \"sub/../evil.wav\", \"hash\": \"" + hexHash ('b') + "\"},"
                      + "{\"path\": \"C.wav\", \"hash\": \"\"},"
                      + "{\"path\": \"/rooted.wav\", \"hash\": \"" + hexHash ('d') + "\"},"
                      + "{\"path\": \"relative/external.wav\", \"hash\": \"" + hexHash ('e') + "\", \"external\": true},"
                      + "{\"path\": \"C:foo\", \"hash\": \"" + hexHash ('f') + "\", \"external\": true},"
                      + "{\"path\": \"" + dotted + "\", \"hash\": \"" + hexHash ('0') + "\", \"external\": true},"
                      + "{\"path\": \"" + absolute + "\", \"hash\": \"" + hexHash ('1') + "\", \"external\": 1},"
                      + "{\"path\": \"" + absolute + "\", \"hash\": \"" + hexHash ('2') + "\", \"external\": true},"
                      + "{\"path\": \"D.wav\", \"hash\": \"" + hexHash ('3') + "\"}"
                      + "]}";

    const auto file = writeFile (directory, "unsafe.json", json);

    std::vector<LibraryEntry> entries;
    REQUIRE (LibraryIndex::load (file, entries));
    REQUIRE (entries.size() == 2);
    CHECK (entries[0].external);
    CHECK (entries[0].path == absolute.toStdString());
    CHECK_FALSE (entries[1].external);
    CHECK (entries[1].path == "D.wav");

    const auto notArray = writeFile (directory, "not-array.json",
        "{\"schemaVersion\": 1, \"samples\": 7}");

    entries.push_back (makeEntry ("stale.wav", kAbcHash));
    REQUIRE (LibraryIndex::load (notArray, entries));
    CHECK (entries.empty());
}

TEST_CASE ("library index rejects entries whose hash is not 64 lowercase hex")
{
    const auto directory = makeLibraryDirectory();
    const auto valid = hexHash ('a');

    const auto file = writeFile (directory, "hashes.json",
        "{\"schemaVersion\": 1, \"samples\": ["
        "{\"path\": \"A.wav\", \"hash\": \"abc\"},"
        "{\"path\": \"B.wav\", \"hash\": \"" + valid.toUpperCase() + "\"},"
        "{\"path\": \"C.wav\", \"hash\": \"" + hexHash ('z') + "\"},"
        "{\"path\": \"D.wav\", \"hash\": \"\"},"
        "{\"path\": \"E.wav\", \"hash\": \"" + valid + "\"}"
        "]}");

    std::vector<LibraryEntry> entries;
    REQUIRE (LibraryIndex::load (file, entries));
    REQUIRE (entries.size() == 1);
    CHECK (entries[0].path == "E.wav");
    CHECK (entries[0].fileHash == valid.toStdString());
}

TEST_CASE ("library index drops thumbnails outside the thumbnails directory")
{
    const auto directory = makeLibraryDirectory();
    const auto hash = hexHash ('b');

    const auto file = writeFile (directory, "thumbnails.json",
        "{\"schemaVersion\": 1, \"samples\": ["
        "{\"path\": \"A.wav\", \"hash\": \"" + hash + "\", \"thumbnail\": \"thumbnails/a.png\"},"
        "{\"path\": \"B.wav\", \"hash\": \"" + hash + "\", \"thumbnail\": \"../outside.png\"},"
        "{\"path\": \"C.wav\", \"hash\": \"" + hash + "\", \"thumbnail\": \"Piano/thumbs/c.png\"},"
        "{\"path\": \"D.wav\", \"hash\": \"" + hash + "\", \"thumbnail\": \"/rooted.png\"},"
        "{\"path\": \"E.wav\", \"hash\": \"" + hash + "\", \"thumbnail\": \"thumbnails\\\\e.png\"}"
        "]}");

    std::vector<LibraryEntry> entries;
    REQUIRE (LibraryIndex::load (file, entries));
    REQUIRE (entries.size() == 5);   // the entries stay; only unsafe thumbnails are dropped

    CHECK (entries[0].thumbnailPath == "thumbnails/a.png");
    CHECK (entries[1].thumbnailPath.empty());
    CHECK (entries[2].thumbnailPath.empty());
    CHECK (entries[3].thumbnailPath.empty());
    CHECK (entries[4].thumbnailPath == "thumbnails/e.png");   // backslashes normalised
}

TEST_CASE ("scan tracks external entries and reports them missing")
{
    const auto directory = makeLibraryDirectory();
    const auto external = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVExternalSample.wav");
    REQUIRE (external.replaceWithText ("abc"));

    LibraryEntry entry;
    entry.path = external.getFullPathName().replaceCharacter ('\\', '/').toStdString();
    entry.fileHash = kAbcHash;
    entry.external = true;

    SampleLibrary library;
    library.setRootDirectory (directory);
    library.setEntries ({ entry });

    auto result = library.scanNow();
    REQUIRE (result.succeeded);
    CHECK (result.missingPaths.empty());
    REQUIRE (library.getEntries().size() == 1);
    CHECK (library.getEntries()[0].external);

    REQUIRE (external.replaceWithText ("def"));
    result = library.scanNow();
    CHECK (result.entriesUpdated == 1);
    CHECK (library.getEntries()[0].fileHash != kAbcHash);

    REQUIRE (external.deleteFile());
    result = library.scanNow();
    REQUIRE (result.missingPaths.size() == 1);
    CHECK (result.missingPaths[0] == entry.path);
    CHECK (library.getEntries().size() == 1);

    REQUIRE (external.replaceWithText ("abc"));
    result = library.scanNow();
    CHECK (result.missingPaths.empty());
    CHECK (result.entriesUpdated == 1);
    CHECK (library.getEntries()[0].fileHash == kAbcHash);
}

TEST_CASE ("scan dedup keeps the first listed entry across internal and external")
{
    const auto directory = makeLibraryDirectory();
    writeFile (directory, "A.wav", "abc");

    const auto external = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVExternalDup.wav");
    REQUIRE (external.replaceWithText ("abc"));

    LibraryEntry externalEntry;
    externalEntry.path = external.getFullPathName().replaceCharacter ('\\', '/').toStdString();
    externalEntry.fileHash = kAbcHash;
    externalEntry.external = true;

    SampleLibrary externalFirst;
    externalFirst.setRootDirectory (directory);
    externalFirst.setEntries ({ externalEntry });
    const auto result = externalFirst.scanNow();
    CHECK (result.duplicatesSkipped == 1);
    REQUIRE (externalFirst.getEntries().size() == 1);
    CHECK (externalFirst.getEntries()[0].external);

    LibraryEntry internalEntry;
    internalEntry.path = "A.wav";
    internalEntry.fileHash = kAbcHash;

    SampleLibrary internalFirst;
    internalFirst.setRootDirectory (directory);
    internalFirst.setEntries ({ internalEntry, externalEntry });
    const auto second = internalFirst.scanNow();
    CHECK (second.duplicatesSkipped == 1);
    REQUIRE (internalFirst.getEntries().size() == 1);
    CHECK_FALSE (internalFirst.getEntries()[0].external);

    external.deleteFile();
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
