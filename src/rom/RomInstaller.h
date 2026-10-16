#pragma once

// First-run installer: copies every file from the user's disc image into
// the app data directory so the runtime can serve cdrom0: from the host filesystem.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "IsoReader.h"
#include "platform/TaskProgress.h"

namespace rt
{
    // Identity of the supported build: The Warriors (USA), SLUS-21215 (NTSC, boot ELF entry
    // 0x100008, CRC32 8C3970AE). The boot ELF SHA-1 is the gate that matters.
    inline constexpr const char *kBootElfName = "SLUS_212.15";
    inline constexpr const char *kVolumeId = "THEWARRIORS";
    inline constexpr const char *kBootElfSha1 = "e9cb2cc49aa046b9e494313dce2f5038ed17b2f4";
    inline constexpr uint32_t kBootElfSize = 4903976;

    // The dump this fork supports, by SHA-1 of the form the player keeps it in: a cooked .iso
    // (2048-byte sectors). Other forms (.bin/.chd) are not verified for this title: empty strings
    // never match, so they fall into the "use it anyway" path while the boot ELF gate above stays.
    inline constexpr const char *kImageSha1Bin = "";
    inline constexpr const char *kImageSha1Iso = "3b68f15cc0c831597467b477ecd9c0944b08c17f";
    inline constexpr const char *kImageSha1ChdCd = "";

    enum class ImageCheck
    {
        Verified,   // the good dump
        Unverified, // a CHD whose data is intact (matches its own record) but isn't one of the above
        Mismatch,   // a .bin/.iso that isn't the good dump
        Damaged,    // a CHD whose data no longer matches its own record, or can't be decompressed
        Unreadable,
    };

    // Hashes the whole image (a few seconds for a .bin or .iso; a CHD is decompressed in full),
    // with `progress` showing it. `detail`: what was found, hashes included (for the log).
    ImageCheck verifyImage(const std::filesystem::path &image, TaskProgress &progress, std::string &detail);

    enum class RomCheck
    {
        Ok,
        WrongGame,     // not a Road Trip disc
        UnknownBuild,  // Road Trip disc but boot ELF hash differs (other region/revision)
        Unreadable,
    };

    // Opens the image and checks the boot ELF against the known build.
    RomCheck checkRom(const std::filesystem::path &image, std::string &detail);

    // Original disc location of every extracted file (paths::lbnMap()). The game reads
    // sectors by LBN from a baked-in table, so the runtime needs these to find data.
    std::vector<IsoEntry> readLbnMap();

    // True if discDir() holds a complete extraction.
    bool isInstalled();

    // Extracts the full disc tree into paths::discDir(). Blocking; safe to run on a worker thread.
    // Does not mark `progress` finished on success, so further steps can follow.
    bool installFromRom(const std::filesystem::path &image, TaskProgress &progress);
}
