#include "IO/LibraryIndex.h"

namespace
{
const juce::Identifier kSchemaVersionKey { "schemaVersion" };
const juce::Identifier kSamplesKey { "samples" };
const juce::Identifier kPathKey { "path" };
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
               ? static_cast<std::int64_t> (value)
               : 0;
}

double getDouble (const juce::DynamicObject& object, const juce::Identifier& name, double fallback)
{
    const auto value = object.getProperty (name);
    return value.isInt() || value.isInt64() || value.isDouble() || value.isBool()
               ? static_cast<double> (value)
               : fallback;
}

bool isSafeRelativePath (const juce::String& path)
{
    if (path.isEmpty() || juce::File::isAbsolutePath (path))
        return false;

    juce::StringArray segments;
    segments.addTokens (path, "/\\", "");
    return ! segments.contains ("..");
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
        object->setProperty (kPathKey, juce::String (entry.relativePath));
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
    const auto hash = object->getProperty (kHashKey).toString();

    if (! isSafeRelativePath (path) || hash.isEmpty())
        return false;

    entry = LibraryEntry {};
    entry.relativePath = path.toStdString();
    entry.fileHash = hash.toStdString();
    entry.rootKey = getInt (*object, kRootKeyKey, 60);
    entry.sourceSampleRate = getDouble (*object, kSourceSampleRateKey, 48000.0);
    entry.lengthSamples = getInt64 (*object, kLengthSamplesKey);
    entry.thumbnailPath = object->getProperty (kThumbnailKey).toString().toStdString();

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
