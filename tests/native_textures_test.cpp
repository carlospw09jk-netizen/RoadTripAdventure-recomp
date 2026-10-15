#include "runtime/gs/gs_native_textures.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace ps2x::gs;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

using Words = std::vector<uint64_t>;

static void qw(Words &w, uint64_t lo, uint64_t hi) { w.push_back(lo); w.push_back(hi); }
static void dma(Words &w, uint32_t id, uint32_t qwc) { qw(w, (uint64_t)qwc | ((uint64_t)id << 28), 0); }
static void adTag(Words &w, uint32_t n) { qw(w, n | (1u << 15) | (1ull << 60), 0xE); }
static void ad(Words &w, uint64_t v, uint32_t a) { qw(w, v, a); }
static uint64_t bb(uint32_t sbp, uint32_t sbw, uint32_t spsm, uint32_t dbp, uint32_t dbw, uint32_t dpsm)
{
    return sbp | (uint64_t)sbw << 16 | (uint64_t)spsm << 24 | (uint64_t)dbp << 32 | (uint64_t)dbw << 48 | (uint64_t)dpsm << 56;
}
static uint64_t pos(uint32_t ssax, uint32_t ssay, uint32_t dsax, uint32_t dsay)
{
    return ssax | (uint64_t)ssay << 16 | (uint64_t)dsax << 32 | (uint64_t)dsay << 48;
}
static uint64_t reg(uint32_t w, uint32_t h) { return w | (uint64_t)h << 32; }
static uint64_t tex0(uint32_t tbp, uint32_t tbw, uint32_t psm, uint32_t tw, uint32_t th, uint32_t cbp, uint32_t cpsm,
                     uint32_t csa, uint32_t cld)
{
    return tbp | (uint64_t)tbw << 14 | (uint64_t)psm << 20 | (uint64_t)tw << 26 | (uint64_t)th << 30 | 1ull << 34 |
           (uint64_t)cbp << 37 | (uint64_t)cpsm << 51 | (uint64_t)csa << 56 | (uint64_t)cld << 61;
}

// One CNT: host->local upload of `bytes` (padded to a quadword).
static void cntUpload(Words &w, uint32_t dbp, uint32_t dbw, uint32_t psm, uint32_t rw, uint32_t rh, const std::vector<uint8_t> &bytes)
{
    uint32_t dq = (uint32_t)((bytes.size() + 15) / 16);
    dma(w, 1, 6 + dq);
    adTag(w, 4);
    ad(w, bb(0, 0, 0, dbp, dbw, psm), 0x50);
    ad(w, pos(0, 0, 0, 0), 0x51);
    ad(w, reg(rw, rh), 0x52);
    ad(w, 0, 0x53);
    qw(w, dq | (1u << 15) | (2ull << 58), 0);
    size_t at = w.size();
    w.resize(at + dq * 2, 0);
    std::memcpy(&w[at], bytes.data(), bytes.size());
}
static void cntCopy(Words &w, const NativeLocalCopy &c, bool end)
{
    dma(w, end ? 7 : 1, 5);
    adTag(w, 4);
    ad(w, bb(c.sbp, c.sbw, c.spsm, c.dbp, c.dbw, c.dpsm), 0x50);
    ad(w, pos(c.ssax, c.ssay, c.dsax, c.dsay), 0x51);
    ad(w, reg(c.rrw, c.rrh), 0x52);
    ad(w, 2, 0x53);
}

static std::vector<NativeTransfer> parse(const Words &w)
{
    std::vector<NativeTransfer> out;
    parseUploadChain((const uint8_t *)w.data(), w.size() * 8, out);
    return out;
}
static std::vector<uint8_t> ramp32(uint32_t n, uint32_t tag) // CT32 palette: pixel p = tag | p
{
    std::vector<uint8_t> v(n * 4);
    for (uint32_t p = 0; p < n; ++p) { uint32_t x = tag | p; std::memcpy(&v[p * 4], &x, 4); }
    return v;
}
static NativeUpload up(uint32_t dbp, uint32_t dbw, uint32_t psm, uint32_t w, uint32_t h, const std::vector<uint8_t> &d)
{
    NativeUpload u; u.dbp = dbp; u.dbw = dbw; u.dpsm = psm; u.rrw = w; u.rrh = h; u.data = d.data(); u.size = d.size();
    return u;
}

static void testParse()
{
    std::vector<uint8_t> px(16 * 4 * 4, 0xAB);
    Words w;
    cntUpload(w, 0x243E, 2, kNativePsmCT32, 16, 4, px);
    NativeLocalCopy c{0x243E, 2, 0x13, 0x2482, 2, 0x13, 0, 0, 0, 0, 128, 128};
    cntCopy(w, c, true);
    auto xs = parse(w);
    CHECK(xs.size() == 2);
    if (xs.size() != 2) return;
    const NativeUpload &u = xs[0].upload;
    CHECK(xs[0].kind == NativeTransfer::Upload);
    CHECK(u.dbp == 0x243E && u.dbw == 2 && u.dpsm == 0 && u.rrw == 16 && u.rrh == 4 && u.dsax == 0 && u.size == px.size());
    CHECK(u.data[0] == 0xAB);
    CHECK(xs[1].kind == NativeTransfer::LocalCopy);
    const NativeLocalCopy &k = xs[1].copy;
    CHECK(k.sbp == 0x243E && k.sbw == 2 && k.spsm == 0x13 && k.dbp == 0x2482 && k.dbw == 2 && k.dpsm == 0x13 && k.rrw == 128 && k.rrh == 128);
}

static void testBlocksAndPartial()
{
    std::vector<uint32_t> b;
    CHECK(NativeTextureTable::blockSet(kNativePsmCT32, 100, 1, 0, 0, 16, 2, b));
    CHECK((b == std::vector<uint32_t>{100, 101}));
    NativeTextureTable::blockSet(kNativePsmT8, 0x243E, 2, 0, 0, 128, 128, b);
    CHECK(b.size() == 64 && b.front() == 0x243E && b.back() == 0x243E + 63);
    NativeTextureTable::blockSet(kNativePsmT4, 0, 1, 0, 0, 64, 64, b);
    CHECK((b == std::vector<uint32_t>{0, 1, 2, 3, 4, 5, 6, 7}));

    NativeTextureTable t;
    auto pal = ramp32(32, 0x01000000);
    CHECK(t.applyUpload(up(100, 1, kNativePsmCT32, 16, 2, pal)));
    std::vector<uint8_t> idx(64 * 64 / 2, 0x21); // T4 64x64: indices 1,2 alternating
    CHECK(t.applyUpload(up(101, 1, kNativePsmT4, 64, 64, idx)));
    const Surface *p = t.lookupSurface(100, kNativePsmCT32, 16, 2);
    CHECK(p && p->partial && p->blocksOwned == 1 && p->blocksTotal == 2);
    const Surface *tx = t.lookupSurface(101, kNativePsmT4, 64, 64);
    CHECK(tx && !tx->partial && tx->texels[0] == 1 && tx->texels[1] == 2);
    CHECK(!t.lookupSurface(101, kNativePsmT4, 128, 64));
    CHECK(!t.lookupSurface(101, kNativePsmT8, 64, 64));
    CHECK(t.applyTex0(tex0(101, 1, kNativePsmT4, 6, 6, 100, 0, 0, 1))); // bank 0 survives
    CHECK(t.clut().entries[0] == (0x01000000u | 0));
    CHECK(t.clut().entries[7] == (0x01000000u | 7));
    CHECK(t.clut().entries[8] == (0x01000000u | 16));
    CHECK(!t.applyTex0(tex0(101, 1, kNativePsmT4, 6, 6, 100, 0, 1, 1))); // bank 1 lived in the lost block
    CHECK(!t.clut().valid);
}

static void testClutCsm1AndBanks()
{
    NativeTextureTable t;
    auto pal = ramp32(256, 0x02000000);
    CHECK(t.applyUpload(up(0x2482, 1, kNativePsmCT32, 16, 16, pal)));
    CHECK(t.applyTex0(tex0(0x243E, 2, kNativePsmT8, 7, 7, 0x2482, 0, 0, 1)));
    // entries 8..15 <- linear 16..23, 16..23 <- linear 8..15
    for (uint32_t e = 0; e < 256; ++e)
    {
        uint32_t r = e & 31, p = (r >= 8 && r < 16) ? e + 8 : (r >= 16 && r < 24) ? e - 8 : e;
        if (t.clut().entries[e] != (0x02000000u | p)) { CHECK(false); break; }
    }

    // car parts: T8 CLD=1 loads a CT16 palette, then T4 CLD=0 picks bank CSA.
    std::vector<uint8_t> p16(256 * 2);
    for (uint32_t p = 0; p < 256; ++p) { uint16_t v = 0x8000 | p; std::memcpy(&p16[p * 2], &v, 2); }
    CHECK(t.applyUpload(up(7106 + 20, 1, kNativePsmCT16, 16, 16, p16)));
    std::vector<uint8_t> idx(32 * 32 / 2, 0x53);
    CHECK(t.applyUpload(up(7106, 1, kNativePsmT4, 32, 32, idx)));
    CHECK(t.applyTex0(tex0(0x1BC2, 2, kNativePsmT8, 6, 5, 7126, 2, 0, 1)));
    CHECK(t.clut().cpsm == 2);
    CHECK(t.applyTex0(tex0(7106, 1, kNativePsmT4, 5, 5, 7126, 2, 3, 0)));
    // bank 3 = entries 48..63; entry 48 sits at linear pixel 40
    CHECK(t.clut().entries[48] == (0x8000u | 40));
    const Surface *s = t.lookupSurface(7106, kNativePsmT4, 32, 32);
    std::vector<uint8_t> rgba;
    CHECK(s && t.decodeRgba(*s, Tex0::fromReg(tex0(7106, 1, kNativePsmT4, 5, 5, 7126, 2, 3, 0)), rgba));
    if (s && rgba.size() >= 8)
    {
        // texel 0 = index 3 -> entry 51, linear 51 (51&31=19 -> 11): value 0x8000|(51-8)
        uint32_t v = 0x8000u | (51 - 8);
        CHECK(rgba[0] == ((v & 31) << 3) && rgba[3] == 0x40); // A bit set -> ta1 (64)
        // texel 1 = index 5 -> entry 53 -> linear 45
        v = 0x8000u | (53 - 8);
        CHECK(rgba[4] == ((v & 31) << 3));
        uint64_t h = NativeTextureTable::hashRgba(32, 32, rgba.data(), rgba.size());
        // independent FNV-1a
        uint64_t e = 1469598103934665603ull; // TextureTools::contentHash's basis
        auto mix = [&](const uint8_t *q, size_t n) { for (size_t i = 0; i < n; ++i) { e ^= q[i]; e *= 0x100000001b3ULL; } };
        uint32_t d[2] = {32, 32};
        mix((const uint8_t *)d, 8); mix(rgba.data(), rgba.size());
        CHECK(h == e);
    }
    // TEXA: aem black -> alpha 0, CT16 without A bit -> ta0
    t.setTexa(0x40ull << 32 | 1u << 15 | 0x20);
    std::vector<uint8_t> r2;
    CHECK(t.decodeRgba(*s, Tex0::fromReg(tex0(7106, 1, kNativePsmT4, 5, 5, 7126, 2, 3, 0)), r2));
    CHECK(r2[3] == 0x40);
}

static void testCopies()
{
    NativeTextureTable t;
    // car slot copy: T8 128x128 + CT32 palette at +0x40
    const uint32_t a = 0x243E, b = 0x243E + 3 * 0x44;
    std::vector<uint8_t> img(128 * 128), pal = ramp32(256, 0x03000000);
    for (size_t i = 0; i < img.size(); ++i) img[i] = (uint8_t)(i * 7);
    CHECK(t.applyUpload(up(a, 2, kNativePsmT8, 128, 128, img)));
    CHECK(t.applyUpload(up(a + 0x40, 1, kNativePsmCT32, 16, 16, pal)));
    uint64_t h0 = t.lookupSurface(a, kNativePsmT8, 128, 128)->contentHash;
    CHECK(t.applyCopy({a, 2, 0x13, b, 2, 0x13, 0, 0, 0, 0, 128, 128}));
    CHECK(t.applyCopy({a + 0x40, 1, 0, b + 0x40, 1, 0, 0, 0, 0, 0, 16, 16}));
    const Surface *c = t.lookupSurface(b, kNativePsmT8, 128, 128);
    CHECK(c && c->texels == img && c->contentHash == h0 && c->gen != t.lookupSurface(a, kNativePsmT8, 128, 128)->gen);
    CHECK(t.lookupSurface(b + 0x40, kNativePsmCT32, 16, 16));

    // texAnimTable: T4 64x256 strip -> T4 64x64 window, scrolling
    const uint32_t strip = 12773, dst = 12755;
    std::vector<uint8_t> sp(64 * 256 / 2);
    for (size_t i = 0; i < sp.size(); ++i) sp[i] = (uint8_t)(((i * 2) / 64 / 4 & 0xF) | ((((i * 2 + 1) / 64 / 4) & 0xF) << 4));
    std::vector<uint8_t> dp(64 * 64 / 2, 0);
    CHECK(t.applyUpload(up(strip, 1, kNativePsmT4, 64, 256, sp)));
    CHECK(t.applyUpload(up(dst, 1, kNativePsmT4, 64, 64, dp)));
    const Surface *d = t.lookupSurface(dst, kNativePsmT4, 64, 64);
    uint32_t g = d->gen;
    CHECK(t.applyCopy({strip, 1, 0x14, dst, 1, 0x14, 0, 8, 0, 0, 64, 64}));
    d = t.lookupSurface(dst, kNativePsmT4, 64, 64);
    CHECK(d && d->gen > g && d->texels[0] == ((8 / 4) & 0xF) && d->texels[63 * 64] == ((71 / 4) & 0xF));
    uint32_t g2 = d->gen; uint64_t h1 = d->contentHash;
    CHECK(t.applyCopy({strip, 1, 0x14, dst, 1, 0x14, 0, 40, 0, 0, 64, 64}));
    d = t.lookupSurface(dst, kNativePsmT4, 64, 64);
    CHECK(d->gen > g2 && d->contentHash != h1 && d->texels[0] == ((40 / 4) & 0xF));
    // partial destination window (64x16 chunk)
    CHECK(t.applyCopy({strip, 1, 0x14, dst, 1, 0x14, 0, 0, 0, 48, 64, 16}));
    // out of bounds source -> fallback, destination disowned
    CHECK(!t.applyCopy({strip, 1, 0x14, dst, 1, 0x14, 0, 250, 0, 0, 64, 64}));
    CHECK(!t.lookupSurface(dst, kNativePsmT4, 64, 64));
    CHECK(t.lookupSurface(strip, kNativePsmT4, 64, 256));
    // copy from an unknown source -> fallback
    CHECK(!t.applyCopy({9000, 1, 0x14, dst, 1, 0x14, 0, 0, 0, 0, 8, 8}));
}

static void testReview()
{
    // hash basis: empty input leaves the basis plus the two size words
    CHECK(NativeTextureTable::hashRgba(0, 0, nullptr, 0) != 0xcbf29ce484222325ULL);
    {
        uint64_t e = 1469598103934665603ull;
        const uint8_t z[8] = {};
        for (uint8_t b : z) { e ^= b; e *= 1099511628211ull; }
        CHECK(NativeTextureTable::hashRgba(0, 0, nullptr, 0) == e);
    }
    // short upload rejected, and it still kills the old surface
    NativeTextureTable t;
    std::vector<uint8_t> full(64 * 64 / 2, 0x11), shortb(10, 0x22);
    CHECK(t.applyUpload(up(500, 1, kNativePsmT4, 64, 64, full)));
    CHECK(!t.applyUpload(up(500, 1, kNativePsmT4, 64, 64, shortb)));
    CHECK(!t.lookupSurface(500, kNativePsmT4, 64, 64));
    CHECK(t.applyUpload(up(500, 1, kNativePsmT4, 64, 64, full)));
    CHECK(!t.applyUpload(up(500, 1, kNativePsmT4, 0, 64, full)));
    // unsupported PSM (CT16S 0x0A) invalidates everything
    CHECK(t.applyUpload(up(600, 1, kNativePsmT4, 64, 64, full)));
    CHECK(!t.applyUpload(up(600, 1, 0x0A, 16, 16, full)));
    CHECK(!t.lookupSurface(600, kNativePsmT4, 64, 64));
    // copy with an unsupported destination PSM
    CHECK(t.applyUpload(up(700, 1, kNativePsmT4, 64, 64, full)));
    CHECK(!t.applyCopy({700, 1, 0x14, 800, 1, 0x0A, 0, 0, 0, 0, 8, 8}));
    CHECK(!t.lookupSurface(700, kNativePsmT4, 64, 64));
    // copy within one surface -> fallback
    CHECK(t.applyUpload(up(700, 1, kNativePsmT4, 64, 64, full)));
    CHECK(!t.applyCopy({700, 1, 0x14, 700, 1, 0x14, 0, 0, 0, 32, 8, 8}));
}

int main()
{
    testReview();
    testParse();
    testBlocksAndPartial();
    testClutCsm1AndBanks();
    testCopies();
    std::printf(g_fail ? "native_textures_test: %d FAILED\n" : "native_textures_test: all passed\n", g_fail);
    return g_fail != 0;
}
