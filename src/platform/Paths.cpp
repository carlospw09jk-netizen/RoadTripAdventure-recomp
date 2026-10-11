#include "Paths.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <dlfcn.h>
#endif

namespace rt::paths
{
    namespace
    {
#if defined(_WIN32)
        // The folder RoadTrip.exe is in.
        std::filesystem::path exeDir()
        {
            std::wstring buf(32768, wchar_t(0));
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (n == 0 || n >= buf.size())
                return {};
            buf.resize(n);
            return std::filesystem::path(buf).parent_path();
        }

        // Decided once per run: the marker isn't looked for again on every path query.
        const DataRootChoice &windowsChoice()
        {
            static const DataRootChoice choice = []
            {
                std::filesystem::path fallback = std::filesystem::path(".") / "RoadTripRecomp";
                // %LOCALAPPDATA%/RoadTripRecomp (wide: the user's name may not be ASCII)
                if (const wchar_t *local = _wgetenv(L"LOCALAPPDATA"); local && *local)
                    fallback = std::filesystem::path(local) / "RoadTripRecomp";
                return chooseDataRoot(std::getenv("RT_DATA_DIR"), exeDir(), fallback);
            }();
            return choice;
        }
#endif
    }

    DataRootChoice chooseDataRoot(const char *rtDataDir, const std::filesystem::path &exeDir,
                                  const std::filesystem::path &platformDefault)
    {
        if (rtDataDir && *rtDataDir)
            return {std::filesystem::path(rtDataDir), false};
        std::error_code ec;
        if (!exeDir.empty() && std::filesystem::is_regular_file(exeDir / kPortableMarker, ec))
            return {exeDir / kPortableDataDir, true};
        return {platformDefault, false};
    }

    bool isWritableDir(const std::filesystem::path &dir)
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!std::filesystem::is_directory(dir, ec))
            return false;
        const std::filesystem::path probe = dir / ".write-test";
        bool ok = false;
        {
            std::ofstream f(probe, std::ios::binary | std::ios::trunc);
            ok = f && (f << "ok") && f.flush();
        }
        std::filesystem::remove(probe, ec);
        return ok;
    }

    bool portable()
    {
#if defined(_WIN32)
        return windowsChoice().portable;
#else
        return false;
#endif
    }

    bool preparePortable(std::string &error)
    {
#if defined(_WIN32)
        if (!portable())
            return true;
        const std::filesystem::path root = dataRoot();
        if (!isWritableDir(root))
        {
            error = std::string("This is a portable copy of Road Trip (") + kPortableMarker +
                    " is next to RoadTrip.exe): it keeps its settings, saves and the game it builds in\n\n" +
                    root.string() +
                    "\n\nbut it can't write there. Move the Road Trip folder somewhere you can write to (Documents or "
                    "a games folder, not Program Files), or delete " +
                    kPortableMarker + " to keep the data in %LOCALAPPDATA%\\RoadTripRecomp instead.";
            return false;
        }
        // Temporary files (clang's and lld's while the game is built, their response files, the
        // upscaler plugins') stay in the folder too: GetTempPath (and so temp_directory_path) and
        // child processes read the process environment.
        std::error_code ec;
        const std::filesystem::path tmp = root / "tmp";
        std::filesystem::create_directories(tmp, ec);
        if (!ec)
        {
            SetEnvironmentVariableW(L"TMP", tmp.c_str());
            SetEnvironmentVariableW(L"TEMP", tmp.c_str());
        }
        std::filesystem::current_path(root, ec);
#else
        (void)error;
#endif
        return true;
    }

    std::filesystem::path dataRoot()
    {
#if defined(_WIN32)
        return windowsChoice().root;
#else
        if (const char *overrideDir = std::getenv("RT_DATA_DIR"); overrideDir && *overrideDir)
            return overrideDir;
#endif
#if defined(__APPLE__)
        const char *home = std::getenv("HOME");
        return std::filesystem::path(home ? home : ".") / "Library" / "Application Support" / "RoadTripRecomp";
#elif defined(__ANDROID__)
        const char *internal = SDL_GetAndroidInternalStoragePath(); // the app's files/ directory
        return internal ? std::filesystem::path(internal) : std::filesystem::path(".");
#elif !defined(_WIN32)
        const char *xdg = std::getenv("XDG_DATA_HOME");
        if (xdg && *xdg)
            return std::filesystem::path(xdg) / "RoadTripRecomp";
        const char *home = std::getenv("HOME");
        return std::filesystem::path(home ? home : ".") / ".local" / "share" / "RoadTripRecomp";
#endif
    }

    std::filesystem::path discDir() { return dataRoot() / "disc"; }
    std::filesystem::path savesDir() { return dataRoot() / "saves"; }
    std::filesystem::path installManifest() { return discDir() / ".installed.json"; }
    std::filesystem::path lbnMap() { return discDir() / ".lbn_map.tsv"; }
    std::filesystem::path gameDir() { return dataRoot() / "game"; }

    std::filesystem::path texturesDir()
    {
#if defined(__ANDROID__)
        if (!std::getenv("RT_DATA_DIR"))
            if (const char *external = SDL_GetAndroidExternalStoragePath(); external && *external)
            {
                const std::filesystem::path dir = std::filesystem::path(external) / "textures";
                // Once: what an earlier version kept in the private data moves here.
                std::error_code ec;
                const std::filesystem::path old = dataRoot() / "textures";
                if (std::filesystem::exists(old, ec) && !std::filesystem::exists(dir, ec))
                {
                    std::filesystem::create_directories(dir.parent_path(), ec);
                    std::filesystem::rename(old, dir, ec);
                    if (ec)
                    {
                        std::filesystem::copy(old, dir, std::filesystem::copy_options::recursive, ec);
                        if (!ec)
                            std::filesystem::remove_all(old, ec);
                    }
                }
                return dir;
            }
#endif
        return dataRoot() / "textures";
    }

    std::string displayPath(const std::filesystem::path &path)
    {
        std::string s = path.string();
#if defined(__ANDROID__)
        for (const char *root : {"/storage/emulated/0/", "/sdcard/"})
            if (s.rfind(root, 0) == 0)
                return s.substr(std::char_traits<char>::length(root));
#endif
        return s;
    }

    std::filesystem::path bundleResources()
    {
        if (const char *overrideDir = std::getenv("RT_RESOURCES_DIR"); overrideDir && *overrideDir)
            return overrideDir;
#if defined(__APPLE__)
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
        {
            std::error_code ec;
            const std::filesystem::path exe = std::filesystem::canonical(buf, ec);
            if (!ec)
                return exe.parent_path().parent_path() / "Resources"; // Contents/MacOS/.. -> Contents/Resources
        }
#elif defined(_WIN32)
        if (const std::filesystem::path dir = exeDir(); !dir.empty())
            return dir / "Resources"; // next to RoadTrip.exe
#elif defined(__ANDROID__)
        return dataRoot() / "res"; // extracted from the APK's assets
#endif
        return "Resources";
    }

    std::filesystem::path toolDir()
    {
#if defined(__ANDROID__)
        // The APK's native library directory (extracted, executable): libmain.so and the compiler.
        Dl_info info{};
        if (dladdr(reinterpret_cast<void *>(&toolDir), &info) && info.dli_fname)
            return std::filesystem::path(info.dli_fname).parent_path();
#endif
        return bundleResources();
    }
}
