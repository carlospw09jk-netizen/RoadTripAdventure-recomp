// Authored memory-card save parser/serializer/migrator. See save.h for the
// provenance of every offset (config/game_state.toml + docs/game-mechanics/
// menus-and-save.md "Save layout"). Pure host C++; no PS2/guest code, no game
// data: only synthetic buffers flow through here in tests.

#include "gameplay/save.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rt::native::gameplay {
namespace {

// ---- little-endian accessors (EE R5900 is LE) ----
std::uint16_t rdU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t rdU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
std::uint64_t rdU64(const std::uint8_t* p) {
    return static_cast<std::uint64_t>(rdU32(p)) | (static_cast<std::uint64_t>(rdU32(p + 4)) << 32);
}
void wrU16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
void wrU32(std::uint8_t* p, std::uint32_t v) {
    wrU16(p, static_cast<std::uint16_t>(v));
    wrU16(p + 2, static_cast<std::uint16_t>(v >> 16));
}
void wrU64(std::uint8_t* p, std::uint64_t v) {
    wrU32(p, static_cast<std::uint32_t>(v));
    wrU32(p + 4, static_cast<std::uint32_t>(v >> 32));
}

// Fixed-width string field: bytes up to the first NUL (or the full capacity).
std::string rdFixedStr(const std::uint8_t* p, std::size_t capacity) {
    std::size_t n = 0;
    while (n < capacity && p[n] != 0) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
}
// Inverse: copy what fits, NUL-pad the rest of the field.
void wrFixedStr(std::uint8_t* p, std::size_t capacity, const std::string& s) {
    const std::size_t n = std::min(s.size(), capacity);
    std::memcpy(p, s.data(), n);
    std::memset(p + n, 0, capacity - n);
}

std::string hexRange(std::size_t lo, std::size_t hi) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%04X-0x%04X", static_cast<unsigned>(lo), static_cast<unsigned>(hi));
    return buf;
}

}  // namespace

const std::vector<SaveFieldRegion>& saveFieldRegions() {
    // Sorted by offset, non-overlapping (currency_name is modeled 18 bytes so
    // it does not swallow licence @0x651; 0x652..0x653 stays unknown padding).
    static const std::vector<SaveFieldRegion> kRegions = {
        {save_offset::kParts,           kPartsCount,            "parts"},
        {save_offset::kPhotosTaken,     16,                     "photos_taken"},
        {save_offset::kStampsEarned,    16,                     "stamps_earned"},
        {save_offset::kTownEntered,     4,                      "town_entered"},
        {save_offset::kStoryFlags,      16,                     "story_flags/scene"},
        {save_offset::kCoinsMissing,    16,                     "coins_missing"},
        {save_offset::kMailInbox,       128,                    "mail_inbox"},
        {save_offset::kMailCount,       4,                      "mail_count"},
        {save_offset::kPlayerName,      kPlayerNameSize,        "player_name"},
        {save_offset::kCurrencyName,    kCurrencyNameSize,      "currency_name"},
        {save_offset::kLicence,         1,                      "licence"},
        {save_offset::kMoney,           4,                      "money"},
        {save_offset::kMileageM,        4,                      "mileage_m"},
        {save_offset::kWgpTeammate0,    1,                      "wgp_teammate_0"},
        {save_offset::kWgpTeammate1,    1,                      "wgp_teammate_1"},
        {save_offset::kUnvisitedDoors,  4 * kLocationCount,     "unvisited_doors"},
        {save_offset::kRaceResults,     kRaceResultCount,       "race_results"},
        {save_offset::kBestLap,         4 * kBestTimeEntries,   "best_lap"},
        {save_offset::kBestTotal,       4 * kBestTimeEntries,   "best_total"},
        {save_offset::kSkiJumpBest,     4,                      "ski_jump_best"},
        {save_offset::kTownsUnvisited,  2,                      "towns_unvisited"},
        {save_offset::kLocation,        1,                      "location"},
        {save_offset::kRadioStation,    1,                      "radio_station"},
        {save_offset::kLastPhoto,       2,                      "last_photo"},
        {save_offset::kControllerRemap, 2 * kControllerActionCount, "controller_remap"},
        {save_offset::kStampQueueCount, 4,                      "stamp_queue_count"},
        {save_offset::kWgpStagesRun,    4,                      "wgp_stages_run"},
    };
    return kRegions;
}

std::uint32_t SaveData::scene() const {
    return static_cast<std::uint32_t>(storyFlags[0]) | (static_cast<std::uint32_t>(storyFlags[1]) << 8) |
           (static_cast<std::uint32_t>(storyFlags[2]) << 16) | (static_cast<std::uint32_t>(storyFlags[3]) << 24);
}

void SaveData::setScene(std::uint32_t v) {
    storyFlags[0] = static_cast<std::uint8_t>(v);
    storyFlags[1] = static_cast<std::uint8_t>(v >> 8);
    storyFlags[2] = static_cast<std::uint8_t>(v >> 16);
    storyFlags[3] = static_cast<std::uint8_t>(v >> 24);
}

SaveResult parseSave(std::span<const std::uint8_t> bytes) {
    SaveResult res;
    if (bytes.size() != kSaveSize) {
        // Size-only validation: the block carries no documented magic/version
        // word (identity lives in the BASLUS-20398 directory entry + icon.sys).
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "invalid save size: got %u bytes, want %u (0x3448 progress block)",
                      static_cast<unsigned>(bytes.size()), static_cast<unsigned>(kSaveSize));
        res.errors.emplace_back(msg);
        return res;
    }

    SaveData& d = res.data;
    std::memcpy(d.raw.data(), bytes.data(), kSaveSize);
    const std::uint8_t* p = d.raw.data();

    std::memcpy(d.parts.data(), p + save_offset::kParts, kPartsCount);
    std::memcpy(d.photosTaken.data(), p + save_offset::kPhotosTaken, d.photosTaken.size());
    d.stampsEarned[0] = rdU64(p + save_offset::kStampsEarned);
    d.stampsEarned[1] = rdU64(p + save_offset::kStampsEarned + 8);
    d.townEntered = rdU32(p + save_offset::kTownEntered);
    std::memcpy(d.storyFlags.data(), p + save_offset::kStoryFlags, d.storyFlags.size());
    std::memcpy(d.coinsMissing.data(), p + save_offset::kCoinsMissing, d.coinsMissing.size());
    std::memcpy(d.mailInbox.data(), p + save_offset::kMailInbox, d.mailInbox.size());
    d.mailCount = rdU32(p + save_offset::kMailCount);
    d.playerName = rdFixedStr(p + save_offset::kPlayerName, kPlayerNameSize);
    d.currencyName = rdFixedStr(p + save_offset::kCurrencyName, kCurrencyNameSize);
    d.licence = p[save_offset::kLicence];
    d.money = rdU32(p + save_offset::kMoney);
    d.mileageM = rdU32(p + save_offset::kMileageM);
    d.wgpTeammates[0] = p[save_offset::kWgpTeammate0];
    d.wgpTeammates[1] = p[save_offset::kWgpTeammate1];
    for (std::size_t i = 0; i < kLocationCount; ++i)
        d.unvisitedDoors[i] = rdU32(p + save_offset::kUnvisitedDoors + 4 * i);
    std::memcpy(d.raceResults.data(), p + save_offset::kRaceResults, kRaceResultCount);
    for (std::size_t i = 0; i < kBestTimeEntries; ++i) {
        d.bestLap[i] = rdU32(p + save_offset::kBestLap + 4 * i);
        d.bestTotal[i] = rdU32(p + save_offset::kBestTotal + 4 * i);
    }
    d.skiJumpBest = rdU32(p + save_offset::kSkiJumpBest);
    d.townsUnvisited = rdU16(p + save_offset::kTownsUnvisited);
    d.location = p[save_offset::kLocation];
    d.radioStation = p[save_offset::kRadioStation];
    d.lastPhoto = rdU16(p + save_offset::kLastPhoto);
    for (std::size_t i = 0; i < kControllerActionCount; ++i)
        d.controllerRemap[i] = rdU16(p + save_offset::kControllerRemap + 2 * i);
    d.stampQueueCount = rdU32(p + save_offset::kStampQueueCount);
    d.wgpStagesRun = rdU32(p + save_offset::kWgpStagesRun);

    // Unknown = complement of the documented regions inside [0, kSaveSize).
    std::size_t cursor = 0;
    for (const SaveFieldRegion& r : saveFieldRegions()) {
        if (r.offset > cursor) res.unknownFields.push_back(hexRange(cursor, r.offset - 1));
        cursor = std::max(cursor, r.offset + r.size);
    }
    if (cursor < kSaveSize) res.unknownFields.push_back(hexRange(cursor, kSaveSize - 1));

    // Known-but-unpinned sub-regions and format caveats, reported explicitly.
    res.unknownFields.emplace_back(
        "note: 0x3304 queued-stamp entries (up to 8 u16 per [stamps]) not modeled - extent not "
        "pinned (doc span collides with results_offset 0x3310)");
    res.unknownFields.emplace_back(
        "note: 0x3314 WGP per-stage result records (8 teams x 3 cars points per "
        "[world_grand_prix]) not modeled - record size not pinned");
    res.unknownFields.emplace_back(
        "note: 0x0652-0x0653 unexplained padding - config currency_name length 21 (0x63F..0x653) "
        "overlaps confirmed licence u8 @0x651; currency_name modeled as 18 bytes");
    res.unknownFields.emplace_back(
        "note: no in-block magic/version documented (identity = BASLUS-20398 dir entry + "
        "icon.sys); parse validates size only");
    res.unknownFields.emplace_back(
        "note: per-port 0x177FD8C strings == player_name @0x62C; per-slot timer 0x177FDB4 == "
        "money @0x654; DAT_017804D0/PTR_DAT_002ED724 slot-pointer tables are runtime state, "
        "not save fields");

    res.ok = true;
    return res;
}

std::vector<std::uint8_t> serializeSave(const SaveData& save) {
    // Start from the retained image so unknown regions stay byte-identical
    // (zeros for a default-constructed SaveData), then overlay documented fields.
    std::vector<std::uint8_t> out(save.raw.begin(), save.raw.end());
    std::uint8_t* p = out.data();

    std::memcpy(p + save_offset::kParts, save.parts.data(), kPartsCount);
    std::memcpy(p + save_offset::kPhotosTaken, save.photosTaken.data(), save.photosTaken.size());
    wrU64(p + save_offset::kStampsEarned, save.stampsEarned[0]);
    wrU64(p + save_offset::kStampsEarned + 8, save.stampsEarned[1]);
    wrU32(p + save_offset::kTownEntered, save.townEntered);
    std::memcpy(p + save_offset::kStoryFlags, save.storyFlags.data(), save.storyFlags.size());
    std::memcpy(p + save_offset::kCoinsMissing, save.coinsMissing.data(), save.coinsMissing.size());
    std::memcpy(p + save_offset::kMailInbox, save.mailInbox.data(), save.mailInbox.size());
    wrU32(p + save_offset::kMailCount, save.mailCount);
    wrFixedStr(p + save_offset::kPlayerName, kPlayerNameSize, save.playerName);
    wrFixedStr(p + save_offset::kCurrencyName, kCurrencyNameSize, save.currencyName);
    p[save_offset::kLicence] = save.licence;
    wrU32(p + save_offset::kMoney, save.money);
    wrU32(p + save_offset::kMileageM, save.mileageM);
    p[save_offset::kWgpTeammate0] = save.wgpTeammates[0];
    p[save_offset::kWgpTeammate1] = save.wgpTeammates[1];
    for (std::size_t i = 0; i < kLocationCount; ++i)
        wrU32(p + save_offset::kUnvisitedDoors + 4 * i, save.unvisitedDoors[i]);
    std::memcpy(p + save_offset::kRaceResults, save.raceResults.data(), kRaceResultCount);
    for (std::size_t i = 0; i < kBestTimeEntries; ++i) {
        wrU32(p + save_offset::kBestLap + 4 * i, save.bestLap[i]);
        wrU32(p + save_offset::kBestTotal + 4 * i, save.bestTotal[i]);
    }
    wrU32(p + save_offset::kSkiJumpBest, save.skiJumpBest);
    wrU16(p + save_offset::kTownsUnvisited, save.townsUnvisited);
    p[save_offset::kLocation] = save.location;
    p[save_offset::kRadioStation] = save.radioStation;
    wrU16(p + save_offset::kLastPhoto, save.lastPhoto);
    for (std::size_t i = 0; i < kControllerActionCount; ++i)
        wrU16(p + save_offset::kControllerRemap + 2 * i, save.controllerRemap[i]);
    wrU32(p + save_offset::kStampQueueCount, save.stampQueueCount);
    wrU32(p + save_offset::kWgpStagesRun, save.wgpStagesRun);

    return out;
}

std::vector<std::string> migrateToNative(SaveData& s) {
    std::vector<std::string> changes;
    auto clampU8 = [&changes](std::uint8_t& v, std::uint8_t maxV, const char* name) {
        if (v > maxV) {
            char msg[96];
            std::snprintf(msg, sizeof(msg), "%s %u -> %u (out of documented range)", name,
                          static_cast<unsigned>(v), static_cast<unsigned>(maxV));
            v = maxV;
            changes.emplace_back(msg);
        }
    };

    clampU8(s.licence, 3, "licence");              // 0 none .. 3 Super A
    clampU8(s.radioStation, 2, "radio_station");   // 0 off, 1 PEACH FM, 2 E-RADIO
    clampU8(s.location, 9, "location");            // 1 Peach Town .. 9 My City

    if (s.money > kMoneyCap) {  // SpendTimerBudget 0x23DC58 cap (money == DAT_0177FDB4)
        char msg[96];
        std::snprintf(msg, sizeof(msg), "money %u -> %u (SpendTimerBudget cap)", s.money, kMoneyCap);
        s.money = kMoneyCap;
        changes.emplace_back(msg);
    }
    if (s.mailCount > 128) {  // inbox holds 128 slots
        char msg[96];
        std::snprintf(msg, sizeof(msg), "mail_count %u -> 128 (inbox capacity)", s.mailCount);
        s.mailCount = 128;
        changes.emplace_back(msg);
    }
    if (s.stampQueueCount > 8) {  // "up to 8 stamp numbers" queued ([stamps])
        char msg[96];
        std::snprintf(msg, sizeof(msg), "stamp_queue_count %u -> 8 (queue capacity)", s.stampQueueCount);
        s.stampQueueCount = 8;
        changes.emplace_back(msg);
    }
    if (s.wgpStagesRun > 7) {  // [world_grand_prix] has 7 stages
        char msg[96];
        std::snprintf(msg, sizeof(msg), "wgp_stages_run %u -> 7 (stage count)", s.wgpStagesRun);
        s.wgpStagesRun = 7;
        changes.emplace_back(msg);
    }
    if (s.lastPhoto > 99) {  // 100 photos, index n-1
        char msg[96];
        std::snprintf(msg, sizeof(msg), "last_photo %u -> 99 (photo index range)", s.lastPhoto);
        s.lastPhoto = 99;
        changes.emplace_back(msg);
    }
    if (s.townsUnvisited & ~static_cast<std::uint16_t>(kTownsAllUnvisited)) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "towns_unvisited 0x%03X -> 0x%03X (10 town bits)",
                      static_cast<unsigned>(s.townsUnvisited),
                      static_cast<unsigned>(s.townsUnvisited & kTownsAllUnvisited));
        s.townsUnvisited &= kTownsAllUnvisited;
        changes.emplace_back(msg);
    }

    // Names: drop anything past the field capacity or an embedded NUL.
    auto fixName = [&changes](std::string& name, std::size_t capacity, const char* label) {
        const std::size_t nul = name.find('\0');
        if (nul != std::string::npos && nul < name.size()) {
            name.resize(nul);
            changes.emplace_back(std::string(label) + " truncated at embedded NUL");
        }
        if (name.size() > capacity) {
            char msg[96];
            std::snprintf(msg, sizeof(msg), "%s truncated %u -> %u bytes (field capacity)", label,
                          static_cast<unsigned>(name.size()), static_cast<unsigned>(capacity));
            name.resize(capacity);
            changes.emplace_back(msg);
        }
    };
    fixName(s.playerName, kPlayerNameSize, "player_name");
    fixName(s.currencyName, kCurrencyNameSize, "currency_name");

    return changes;
}

}  // namespace rt::native::gameplay
