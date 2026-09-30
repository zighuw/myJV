#include "IO/LibraryIndex.h"

namespace
{
const juce::Identifier kSchemaVersionKey { "schemaVersion" };
const juce::Identifier kSamplesKey { "samples" };
const juce::Identifier kPathKey { "path" };
const juce::Identifier kExternalKey { "external" };
const juce::Identifier kHashKey { "hash" };
const juce::Identifier kRootKeyKey { "rootKey" };
const juce::Identifier kSourceSampleRateKey { "sourceSampleRate" };
const juce::Identifier kLengthSamplesKey { "lengthSamples" };
const juce::Identifier kThumbnailKey { "thumbnail" };
const juce::Identifier kLoopKey { "loop" };
const juce::Identifier kLoopStartKey { "start" };
const juce::Identifier kLoopEndKey { "end" };
const juce::Identifier kLoopCrossfadeSamplesKey { "crossfadeSamples" };

int getInt (const juce::DynamicObject& object, const juce::Identifier& name, int fallback)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool()
               ? static_cast<int> (value)
               : fallback;
}

std::int64_t getInt64 (const juce::DynamicObject& object, const juce::Identifier& name)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool()
               ? static_cast<std::int64_t> (static_cast<juce::int64> (value))
               : 0;
}

double getDouble (const juce::DynamicObject& object, const juce::Identifier& name, double fallback)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool()
               ? static_cast<double> (value)
               : fallback;
}

bool getBool (const juce::DynamicObject& object, const juce::Identifier& name)
{
    // Fail-closed: only a real JSON boolean is accepted (e.g. "external": 1 is
    // treated as false, so the entry is rejected if its path is absolute).
    const auto value = object.getProperty (name);
    return value.isBool() && static_cast<bool> (value);
}

bool isFullyQualifiedAbsolutePath (const juce::String& path)
{
#if JUCE_WINDOWS
    if (path.length() >= 3 && juce::CharacterFunctions::isLetter (path[0])
        && path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
        return true;

    return path.startsWith ("//") || path.startsWith ("\\\\");   // UNC
#else
    return path.startsWithChar ('/');
#endif
}

bool isSafeEntryPath (const juce::String& path, bool external)
{
    if (path.isEmpty())
        return false;

    juce::StringArray segments;
    segments.addTokens (path, "/\\", "");

    if (segments.contains (".."))
        return false;

    if (external)
        return isFullyQualifiedAbsolutePath (path);

    // Internal entries must stay under the root: reject absolute paths and
    // root-relative Windows paths like "/evil.wav" (juce::File treats them as
    // root-relative and would resolve them outside the library root).
    return ! isFullyQualifiedAbsolutePath (path)
           && ! path.startsWithChar ('/')
           && ! path.startsWithChar ('\\');
}

// The hash is the dedup key and the sample-cache key, so a malformed one (short,
// uppercase or non-hex) can never match anything: the entry is dropped instead of
// being kept in a permanently unmatched state (code-review N-04).
bool isLowercaseHexHash (const juce::String& hash)
{
    if (hash.length() != 64)
        return false;

    for (int i = 0; i < hash.length(); ++i)
    {
        const auto character = hash[i];

        if (! ((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
    }

    return true;
}

juce::var entriesToVar (const std::vector<LibraryEntry>& entries)
{
    juce::Array<juce::var> samples;

    for (const auto& entry : entries)
    {
        auto* loop = new juce::DynamicObject();
        loop->setProperty (kLoopStartKey, entry.loop.start);
        loop->setProperty (kLoopEndKey, entry.loop.end);
        loop->setProperty (kLoopCrossfadeSamplesKey, entry.loop.crossfadeSamples);

        auto* object = new juce::DynamicObject();
        object->setProperty (kPathKey, juce::String (entry.path));
        object->setProperty (kExternalKey, entry.external);
        object->setProperty (kHashKey, juce::String (entry.fileHash));
        object->setProperty (kRootKeyKey, entry.rootKey);
        object->setProperty (kSourceSampleRateKey, entry.sourceSampleRate);
        object->setProperty (kLengthSamplesKey, static_cast<juce::int64> (entry.lengthSamples));
        object->setProperty (kThumbnailKey, juce::String (entry.thumbnailPath));
        object->setProperty (kLoopKey, juce::var (loop));

        samples.add (juce::var (object));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty (kSchemaVersionKey, LibraryIndex::kSchemaVersion);
    root->setProperty (kSamplesKey, samples);
    return juce::var (root);
}

bool varToEntry (const juce::var& value, LibraryEntry& entry)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return false;

    const auto path = object->getProperty (kPathKey).toString();
    const auto external = getBool (*object, kExternalKey);
    const auto hash = object->getProperty (kHashKey).toString();

    if (! isSafeEntryPath (path, external) || ! isLowercaseHexHash (hash))
        return false;

    entry = LibraryEntry {};
    entry.path = path.toStdString();
    entry.external = external;
    entry.fileHash = hash.toStdString();
    entry.rootKey = getInt (*object, kRootKeyKey, 60);
    entry.sourceSampleRate = getDouble (*object, kSourceSampleRateKey, 48000.0);
    entry.lengthSamples = getInt64 (*object, kLengthSamplesKey);

    // Thumbnails are written under <root>/thumbnails/ (SampleImporter) and must
    // stay root-relative: a hand-edited index must not point the waveform view at
    // an arbitrary path. Anything else is dropped, not fatal (code-review N-01).
    const auto thumbnail = object->getProperty (kThumbnailKey).toString().replaceCharacter ('\\', '/');

    if (thumbnail.isNotEmpty() && isSafeEntryPath (thumbnail, false)
        && thumbnail.startsWith (juce::String (kThumbnailsDirectoryName) + "/"))
        entry.thumbnailPath = thumbnail.toStdString();

    if (const auto* loop = object->getProperty (kLoopKey).getDynamicObject())
    {
        entry.loop.start = getInt (*loop, kLoopStartKey, 0);
        entry.loop.end = getInt (*loop, kLoopEndKey, 0);
        entry.loop.crossfadeSamples = getInt (*loop, kLoopCrossfadeSamplesKey, 0);
    }

    return true;
}
}

bool LibraryIndex::load (const juce::File& file, std::vector<LibraryEntry>& entries)
{
    entries.clear();

    if (! file.existsAsFile())
        return false;

    juce::var parsed;
    const auto status = juce::JSON::parse (file.loadFileAsString(), parsed);

    if (status.failed())
        return false;

    const auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    const auto version = root->getProperty (kSchemaVersionKey);

    if (! (version.isInt() || version.isInt64())
        || static_cast<juce::int64> (version) != kSchemaVersion)
        return false;

    const auto samples = root->getProperty (kSamplesKey);

    if (const auto* array = samples.getArray())
    {
        entries.reserve (static_cast<std::size_t> (array->size()));

        for (const auto& value : *array)
        {
            LibraryEntry entry;

            if (varToEntry (value, entry))
                entries.push_back (std::move (entry));
        }
    }

    return true;
}

bool LibraryIndex::save (const juce::File& file, const std::vector<LibraryEntry>& entries)
{
    const auto parent = file.getParentDirectory();

    if (! parent.isDirectory() && parent.createDirectory().failed())
        return false;

    const auto json = juce::JSON::toString (entriesToVar (entries));

    juce::TemporaryFile temporary (file);

    if (! temporary.getFile().replaceWithText (json))
        return false;

    return temporary.overwriteTargetFileWithTemporary();
}
