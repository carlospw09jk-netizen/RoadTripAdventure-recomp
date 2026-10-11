// The data folder's rule (rt::paths::chooseDataRoot): RT_DATA_DIR, then the portable marker beside
// the executable, then the platform's default. Runs on every platform; only Windows uses the marker.

#include "platform/Paths.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace rt::paths;

namespace
{
    int g_failures = 0;
    void check(bool ok, const char *what, int line)
    {
        if (!ok)
        {
            std::fprintf(stderr, "FAIL line %d: %s\n", line, what);
            ++g_failures;
        }
    }
#define CHECK(x) check((x), #x, __LINE__)
}

int main()
{
    const fs::path exe = fs::temp_directory_path() / "rt_paths_test" / "Road Trip";
    const fs::path fallback = fs::temp_directory_path() / "rt_paths_test" / "LocalAppData" / "RoadTripRecomp";
    std::error_code ec;
    fs::remove_all(exe.parent_path(), ec);
    fs::create_directories(exe, ec);

    // No marker: the platform's default.
    DataRootChoice c = chooseDataRoot(nullptr, exe, fallback);
    CHECK(c.root == fallback && !c.portable);
    c = chooseDataRoot("", exe, fallback); // an empty RT_DATA_DIR counts as unset
    CHECK(c.root == fallback && !c.portable);

    // A directory named like the marker isn't the marker.
    fs::create_directories(exe / kPortableMarker, ec);
    c = chooseDataRoot(nullptr, exe, fallback);
    CHECK(c.root == fallback && !c.portable);
    fs::remove(exe / kPortableMarker, ec);

    // The marker: <exe folder>/data.
    std::ofstream(exe / kPortableMarker) << "portable\n";
    c = chooseDataRoot(nullptr, exe, fallback);
    CHECK(c.root == exe / "data" && c.portable);

    // RT_DATA_DIR still wins over the marker.
    c = chooseDataRoot("/somewhere/else", exe, fallback);
    CHECK(c.root == fs::path("/somewhere/else") && !c.portable);

    // No executable folder known: never portable.
    c = chooseDataRoot(nullptr, fs::path(), fallback);
    CHECK(c.root == fallback && !c.portable);

    // The writable check makes the folder and leaves nothing behind.
    CHECK(isWritableDir(exe / "data"));
    CHECK(fs::is_directory(exe / "data"));
    CHECK(fs::is_empty(exe / "data", ec));
    // A path under a regular file can't be made.
    CHECK(!isWritableDir(exe / kPortableMarker / "data"));

    // Only Windows has a portable mode: elsewhere the app never reports it.
#if !defined(_WIN32)
    CHECK(!portable());
    std::string error;
    CHECK(preparePortable(error) && error.empty());
#endif

    fs::remove_all(exe.parent_path(), ec);
    if (g_failures)
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
    else
        std::printf("paths_test: all passed\n");
    return g_failures ? 1 : 0;
}
