#pragma once

#include <filesystem>
#include <string>

namespace rt::paths
{
    // Where everything the app writes lives: $RT_DATA_DIR if set; else on Windows <exe folder>/data
    // when the portable marker (portable.txt) is beside RoadTrip.exe, else %LOCALAPPDATA%/RoadTripRecomp;
    // ~/Library/Application Support/RoadTripRecomp on macOS; the app's files/ on Android.
    std::filesystem::path dataRoot();

    // Portable mode (Windows): the marker file beside the executable, and the data folder next to it.
    inline constexpr const char *kPortableMarker = "portable.txt";
    inline constexpr const char *kPortableDataDir = "data";
    struct DataRootChoice
    {
        std::filesystem::path root;
        bool portable = false;
    };
    // dataRoot()'s rule without the environment: `rtDataDir` ($RT_DATA_DIR, may be null) wins, then
    // <exeDir>/data if <exeDir>/portable.txt exists (and exeDir isn't empty), else `platformDefault`.
    DataRootChoice chooseDataRoot(const char *rtDataDir, const std::filesystem::path &exeDir,
                                  const std::filesystem::path &platformDefault);
    // True when dataRoot() is the portable folder beside the executable (Windows only).
    bool portable();
    // Portable mode only (elsewhere a no-op that returns true): makes the data folder and checks that
    // it is writable (false and `error` if not), points TMP/TEMP into it (the game build's compiler
    // and linker, and anything else asking for a temp file, inherit them) and makes it the working
    // directory, so writes relative to it (libraries, the runtime's debug dumps) stay inside too.
    bool preparePortable(std::string &error);
    // A test write into `dir` (created if missing): true if a file can be created there.
    bool isWritableDir(const std::filesystem::path &dir);
    // Extracted disc tree; acts as cdrom0: for the runtime.
    std::filesystem::path discDir();
    // Memory card root (mc0:).
    std::filesystem::path savesDir();
    // Marker written after a successful extraction.
    std::filesystem::path installManifest();
    // Where the game library is built from the user's disc on first launch.
    std::filesystem::path gameDir();
    // RoadTrip.app/Contents/Resources (recompiler, SDK headers, config). $RT_RESOURCES_DIR overrides.
    std::filesystem::path bundleResources();
    // Tab-separated "path<TAB>lbn<TAB>size" for every file on the disc.
    std::filesystem::path lbnMap();
    // Where shipped executables live (Android: the native library directory, which may run them).
    std::filesystem::path toolDir();
    // Texture dumps and packs (textures/dumps, textures/packs). On Android the app's external files
    // (Android/data/<package>/files), which a computer reaches over USB, unlike the private data.
    std::filesystem::path texturesDir();
    // A path as the player would find it (Android: from the shared storage's root).
    std::string displayPath(const std::filesystem::path &path);
}
