// rt_model_dump <file> <output dir> [--va 0xADDR ...]
//
// Decodes the game's model VIF chains (docs/native-render.md) and writes them as OBJ, plus a JSON
// of chains, batches and materials, for checking the format by eye. A research tool: it reads the
// user's extracted disc (FLD/COURSE/ACTION/CAR*/CARS/SHOP/SYS .BIN) at run time and writes under
// the given directory (keep it under build/: the output is derived from the game).
//
// Without --va the file is scanned for model chains (a CNT DMA tag whose VIF codes start with
// STCYCL 1,1, followed to its RET). With --va the file is the boot ELF and each address is the
// start of a chain in it (the sky and shadow chains live there).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace
{
std::vector<uint8_t> readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

uint32_t u32(const std::vector<uint8_t> &d, size_t o)
{
    if (o + 4 > d.size())
        return 0;
    uint32_t v;
    std::memcpy(&v, &d[o], 4);
    return v;
}

float f32(uint32_t w)
{
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

struct Batch
{
    int entry = -1;          // MSCAL address / 0x10 (the recompiler's entryN)
    uint32_t hdr[4] = {};    // the GIF tag the microprogram outputs (UNPACK V4-32 1 at TOPS+0)
    bool hasHdr = false;
    std::vector<uint32_t> words; // the vertex block (UNPACK V3-32 at TOPS+1, or +0 for the sky)
    uint32_t vecs = 0;
    int material = -1;
    size_t fileOffset = 0;
};

struct Material
{
    std::vector<std::pair<uint32_t, uint64_t>> regs; // (GS register, value) from the A+D block at TOPS+0x192
};

struct Chain
{
    size_t offset = 0;
    std::string label;
    std::vector<Batch> batches;
    std::vector<std::string> notes;
};

struct Model
{
    std::vector<Chain> chains;
    std::vector<Material> materials;
};

// The VIF stream of a chain: tag upper halves and their data, in transfer order.
struct Stream
{
    std::vector<uint8_t> bytes;
    std::vector<size_t> source; // file offset of each 4-byte word
};

bool collectChain(const std::vector<uint8_t> &d, size_t start, long long base, Stream &s, size_t &end)
{
    size_t o = start;
    std::vector<size_t> stack;
    for (int tags = 0; tags < 4096; ++tags)
    {
        if (o + 16 > d.size())
            return false;
        const uint64_t lo = uint64_t(u32(d, o)) | (uint64_t(u32(d, o + 4)) << 32);
        const uint32_t qwc = lo & 0xffff, id = (lo >> 28) & 7, addr = (lo >> 32) & 0x7fffffff;
        auto add = [&](size_t from, size_t n) {
            if (from + n > d.size())
                return false;
            for (size_t i = 0; i < n; i += 4)
            {
                s.bytes.insert(s.bytes.end(), d.begin() + from + i, d.begin() + from + i + 4);
                s.source.push_back(from + i);
            }
            return true;
        };
        if (!add(o + 8, 8))
            return false;
        const size_t target = size_t(base + (long long)addr);
        switch (id)
        {
        case 1: // CNT
            if (!add(o + 16, qwc * 16))
                return false;
            o += 16 + qwc * 16;
            break;
        case 3: // REF
        case 4: // REFS
            if (!add(target, qwc * 16))
                return false;
            o += 16;
            break;
        case 0: // REFE
            end = o + 16;
            return add(target, qwc * 16);
        case 2: // NEXT
            if (!add(o + 16, qwc * 16))
                return false;
            o = target;
            break;
        case 5: // CALL
            if (!add(o + 16, qwc * 16))
                return false;
            stack.push_back(o + 16 + qwc * 16);
            o = target;
            break;
        case 6: // RET
            if (!add(o + 16, qwc * 16))
                return false;
            if (stack.empty())
            {
                end = o + 16 + qwc * 16;
                return true;
            }
            o = stack.back();
            stack.pop_back();
            break;
        default: // END
            end = o + 16 + qwc * 16;
            return add(o + 16, qwc * 16);
        }
    }
    return false;
}

const char *unpackName(uint32_t vnvl)
{
    static const char *names[16] = {"S-32", "S-16", "S-8", "?", "V2-32", "V2-16", "V2-8", "?",
                                    "V3-32", "V3-16", "V3-8", "?", "V4-32", "V4-16", "V4-8", "V4-5"};
    return names[vnvl & 15];
}

// Decodes a chain's VIF stream into batches. Returns false on codes the format never uses.
bool decodeVif(const Stream &s, Chain &c, Model &m)
{
    const auto &b = s.bytes;
    size_t o = 0;
    auto w = [&](size_t at) {
        uint32_t v = 0;
        if (at + 4 <= b.size())
            std::memcpy(&v, &b[at], 4);
        return v;
    };
    Batch cur;
    int material = -1;
    while (o + 4 <= b.size())
    {
        const uint32_t code = w(o);
        const uint32_t cmd = (code >> 24) & 0x7f, num = (code >> 16) & 0xff, imm = code & 0xffff;
        const size_t at = o;
        o += 4;
        if (cmd >= 0x60)
        {
            const uint32_t vnvl = cmd & 0xf;
            static const int comps[16] = {1, 1, 1, 0, 2, 2, 2, 0, 3, 3, 3, 0, 4, 4, 4, 4};
            static const int bits[16] = {32, 16, 8, 0, 32, 16, 8, 0, 32, 16, 8, 0, 32, 16, 8, 5};
            const uint32_t n = num ? num : 256;
            const size_t size = (size_t(comps[vnvl]) * bits[vnvl] * n + 31) / 32 * 4;
            const uint32_t addr = imm & 0x3ff;
            if (cmd & 0x10)
                c.notes.push_back("masked UNPACK");
            if (vnvl == 0xC && n == 1 && addr == 0)
            {
                for (int i = 0; i < 4; ++i)
                    cur.hdr[i] = w(o + 4 * i);
                cur.hasHdr = true;
            }
            else if (vnvl == 0x8 && addr == 0x192)
            {
                // A+D block: a GIF tag then (data64, register) per vector; W is not written.
                Material mat;
                for (uint32_t i = 1; i < n; ++i)
                {
                    const uint64_t v = uint64_t(w(o + 12 * i)) | (uint64_t(w(o + 12 * i + 4)) << 32);
                    mat.regs.push_back({w(o + 12 * i + 8) & 0xff, v});
                }
                m.materials.push_back(mat);
                material = int(m.materials.size()) - 1;
            }
            else if (vnvl == 0x8)
            {
                cur.words.assign(n * 3, 0);
                for (uint32_t i = 0; i < n * 3; ++i)
                    cur.words[i] = w(o + 4 * i);
                cur.vecs = n;
                cur.fileOffset = (o / 4 < s.source.size()) ? s.source[o / 4] : 0;
                if (addr == 0 && !cur.hasHdr)
                {
                    // The sky's blocks start with their GIF tag as a V3 vector.
                    cur.hdr[0] = cur.words[0];
                    cur.hdr[1] = cur.words[1];
                    cur.hdr[2] = cur.words[2];
                    cur.hasHdr = true;
                    cur.words.erase(cur.words.begin(), cur.words.begin() + 3);
                    cur.vecs -= 1;
                }
            }
            else
            {
                c.notes.push_back(std::string("unhandled UNPACK ") + unpackName(vnvl));
            }
            o += size;
            continue;
        }
        switch (cmd)
        {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x10: case 0x11: case 0x13:
            break;
        case 0x14: case 0x15: case 0x17: // MSCAL, MSCALF, MSCNT
            cur.entry = cmd == 0x17 ? -2 : int(imm * 8 / 0x10);
            cur.material = material;
            c.batches.push_back(cur);
            cur = Batch();
            break;
        case 0x20:
            o += 4;
            c.notes.push_back("STMASK");
            break;
        case 0x30: case 0x31:
            o += 16;
            c.notes.push_back(cmd == 0x30 ? "STROW" : "STCOL");
            break;
        case 0x4a:
            o += size_t(num ? num : 256) * 8;
            c.notes.push_back("MPG");
            break;
        case 0x50: case 0x51:
            o += size_t(imm ? imm : 65536) * 16;
            c.notes.push_back("DIRECT");
            break;
        default:
            char buf[64];
            std::snprintf(buf, sizeof buf, "unknown VIF code 0x%08x at +0x%zx", code, at);
            c.notes.push_back(buf);
            return false;
        }
    }
    return true;
}

struct Vtx
{
    float p[3], c[3], st[2];
};

// Vertices of one batch by microprogram (layouts in docs/native-render.md).
std::vector<Vtx> vertices(const Batch &b, uint32_t nloop, std::string &why)
{
    std::vector<Vtx> out;
    auto v = [&](uint32_t vec, int k) { return f32(b.words[vec * 3 + k]); };
    uint32_t stride = 0, first = 0, posVec = 0, colVec = 0, stVec = 0;
    switch (b.entry)
    {
    case 2: case 5: stride = 4; posVec = 0; colVec = 2; stVec = 3; break;     // pos, normal, colour, stq
    case 4: stride = 5; posVec = 0; colVec = 1; stVec = 4; break;             // pos, 3 colour vectors, stq
    case 6: stride = 3; posVec = 0; colVec = 1; stVec = 2; break;             // pos, colour, stq
    case 3: stride = 5; first = 1; posVec = 0; colVec = 1; stVec = 4; break;  // centre, then per corner offset, colours, stq
    default: why = "no layout for this microprogram"; return out;
    }
    if (first + stride * nloop > b.vecs)
    {
        why = "vertex block shorter than NLOOP x stride";
        return out;
    }
    for (uint32_t i = 0; i < nloop; ++i)
    {
        const uint32_t base = first + i * stride;
        Vtx x;
        for (int k = 0; k < 3; ++k)
        {
            x.p[k] = v(base + posVec, k);
            x.c[k] = v(base + colVec, k);
        }
        if (b.entry == 3) // billboard corner: centre + offset (drawn here in the X/Y plane)
            for (int k = 0; k < 3; ++k)
                x.p[k] = v(0, k) + v(base, k);
        x.st[0] = v(base + stVec, 0);
        x.st[1] = v(base + stVec, 1);
        const float q = v(base + stVec, 2);
        if (b.entry != 2 && q != 0.0f && std::isfinite(q))
        {
            x.st[0] /= q;
            x.st[1] /= q;
        }
        out.push_back(x);
    }
    return out;
}

const char *regName(uint32_t r)
{
    switch (r)
    {
    case 0x06: return "TEX0_1"; case 0x07: return "TEX0_2"; case 0x08: return "CLAMP_1"; case 0x09: return "CLAMP_2";
    case 0x14: return "TEX1_1"; case 0x15: return "TEX1_2"; case 0x34: return "MIPTBP1_1"; case 0x35: return "MIPTBP1_2";
    case 0x36: return "MIPTBP2_1"; case 0x37: return "MIPTBP2_2"; case 0x42: return "ALPHA_1"; case 0x43: return "ALPHA_2";
    case 0x47: return "TEST_1"; case 0x48: return "TEST_2";
    default: return nullptr;
    }
}

void labelKnown(const std::string &path, const std::vector<uint8_t> &d, std::map<size_t, std::string> &labels)
{
    char buf[64];
    if (path.find("/FLD/") != std::string::npos || path.find("/COURSE/") != std::string::npos ||
        path.find("/ACTION/") != std::string::npos)
    {
        // Header: u32 header size, then section offsets; section 1 starts with 64 cell chains + 1.
        const uint32_t s1 = u32(d, 4);
        for (uint32_t i = 0; i < 65 && s1 + 4 * i + 4 <= d.size(); ++i)
        {
            std::snprintf(buf, sizeof buf, i < 64 ? "cell %u,%u" : "cell extra", i & 7, i >> 3);
            labels[s1 + u32(d, s1 + 4 * i)] = buf;
        }
    }
    else if (path.find("/CAR") != std::string::npos && path.find("/CARS/") == std::string::npos)
    {
        // Header: 6 model offsets (each a 16-byte model header, its main chain right after).
        static const char *names[6] = {"body", "body far", "variant a", "variant b", "extra a", "extra b"};
        for (int i = 0; i < 6; ++i)
        {
            const uint32_t m = u32(d, 4 * i);
            if (m + 16 > d.size())
                continue;
            labels[m + 16] = names[i];
            for (int part = 0; part < 2; ++part)
                if (u32(d, m + 8 + 4 * part))
                {
                    std::snprintf(buf, sizeof buf, "%s part %c", names[i], 'A' + part);
                    labels[m + (u32(d, m + 4 * part) & ~15u)] = buf;
                }
        }
    }
}

bool elfVaToOffset(const std::vector<uint8_t> &d, uint32_t va, size_t &off)
{
    if (d.size() < 52 || std::memcmp(d.data(), "\x7f" "ELF", 4))
        return false;
    const uint32_t phoff = u32(d, 28);
    const uint32_t phnum = u32(d, 44) & 0xffff;
    for (uint32_t i = 0; i < phnum; ++i)
    {
        const size_t p = phoff + 32 * i;
        const uint32_t type = u32(d, p), offset = u32(d, p + 4), vaddr = u32(d, p + 8), filesz = u32(d, p + 16);
        if (type == 1 && va >= vaddr && va < vaddr + filesz)
        {
            off = offset + (va - vaddr);
            return true;
        }
    }
    return false;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <file> <output dir> [--va 0xADDR ...]\n", argv[0]);
        return 2;
    }
    const std::string path = argv[1], outDir = argv[2];
    std::vector<uint32_t> vas;
    for (int i = 3; i < argc; ++i)
        if (!std::strcmp(argv[i], "--va") && i + 1 < argc)
            vas.push_back(uint32_t(std::strtoul(argv[++i], nullptr, 0)));
    const std::vector<uint8_t> d = readFile(path);
    if (d.empty())
    {
        std::fprintf(stderr, "cannot read %s\n", path.c_str());
        return 1;
    }

    Model model;
    std::map<size_t, std::string> labels;
    labelKnown(path, d, labels);

    auto take = [&](size_t start, long long base, const std::string &label, size_t &end) {
        Stream s;
        Chain c;
        c.offset = start;
        c.label = label;
        if (!collectChain(d, start, base, s, end))
            return false;
        decodeVif(s, c, model);
        if (c.batches.empty())
            return false;
        model.chains.push_back(std::move(c));
        return true;
    };

    if (!vas.empty())
    {
        for (uint32_t va : vas)
        {
            size_t off = 0, end = 0;
            if (!elfVaToOffset(d, va, off))
            {
                std::fprintf(stderr, "0x%x is not in the ELF\n", va);
                continue;
            }
            char label[32];
            std::snprintf(label, sizeof label, "va 0x%x", va);
            if (!take(off, (long long)off - (long long)va, label, end))
                std::fprintf(stderr, "no chain at 0x%x\n", va);
        }
    }
    else
    {
        for (size_t o = 0; o + 16 <= d.size();)
        {
            const uint32_t lo0 = u32(d, o), lo1 = u32(d, o + 4), up0 = u32(d, o + 8);
            size_t end = 0;
            if (((lo0 >> 28) & 7) == 1 && (lo0 & 0xffff) && lo1 == 0 && up0 == 0x01000101 &&
                take(o, 0, labels.count(o) ? labels[o] : std::string(), end))
            {
                o = std::max(o + 16, (end + 15) & ~size_t(15));
                continue;
            }
            o += 16;
        }
    }

    const std::string stem = path.substr(path.find_last_of('/') + 1);
    std::string base = outDir + "/" + stem;
    std::FILE *obj = std::fopen((base + ".obj").c_str(), "w");
    std::FILE *js = std::fopen((base + ".json").c_str(), "w");
    if (!obj || !js)
    {
        std::fprintf(stderr, "cannot write to %s\n", outDir.c_str());
        return 1;
    }
    std::fprintf(obj, "# %s: model chains decoded by rt_model_dump (model space, y up)\n", stem.c_str());
    std::fprintf(js, "{\n  \"file\": \"%s\",\n  \"materials\": [", stem.c_str());
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        std::fprintf(js, "%s\n    {", i ? "," : "");
        for (size_t r = 0; r < model.materials[i].regs.size(); ++r)
        {
            const auto &reg = model.materials[i].regs[r];
            char other[16];
            std::snprintf(other, sizeof other, "reg_0x%02x", reg.first);
            const char *n = regName(reg.first);
            std::fprintf(js, "%s\"%s\": \"0x%016llx\"", r ? ", " : "", n ? n : other, (unsigned long long)reg.second);
        }
        std::fprintf(js, "}");
    }
    std::fprintf(js, "\n  ],\n  \"chains\": [");

    size_t vbase = 1, totalTris = 0, totalBatches = 0, skipped = 0;
    std::map<int, size_t> perEntry;
    for (size_t ci = 0; ci < model.chains.size(); ++ci)
    {
        const Chain &c = model.chains[ci];
        std::fprintf(obj, "o chain_%06zx%s%s\n", c.offset, c.label.empty() ? "" : "_", c.label.c_str());
        std::fprintf(js, "%s\n    {\"offset\": \"0x%zx\", \"label\": \"%s\", \"notes\": [", ci ? "," : "", c.offset, c.label.c_str());
        for (size_t n = 0; n < c.notes.size(); ++n)
            std::fprintf(js, "%s\"%s\"", n ? ", " : "", c.notes[n].c_str());
        std::fprintf(js, "], \"batches\": [");
        for (size_t bi = 0; bi < c.batches.size(); ++bi)
        {
            const Batch &b = c.batches[bi];
            const uint64_t tag = uint64_t(b.hdr[0]) | (uint64_t(b.hdr[1]) << 32);
            const uint32_t nloop = tag & 0x7fff, prim = (tag >> 47) & 0x7ff;
            std::string why;
            const std::vector<Vtx> vs = b.hasHdr ? vertices(b, nloop, why) : std::vector<Vtx>();
            if (!b.hasHdr)
                why = "no GIF tag";
            std::fprintf(js, "%s\n      {\"entry\": %d, \"file_offset\": \"0x%zx\", \"nloop\": %u, \"prim\": \"0x%x\", \"spare_lo\": \"0x%x\", "
                             "\"fan_tag_hi\": \"0x%08x\", \"vectors\": %u, \"material\": %d%s%s%s}",
                         bi ? "," : "", b.entry, b.fileOffset, nloop, prim, b.hdr[1] & 0x3fff, b.hdr[3], b.vecs, b.material,
                         why.empty() ? "" : ", \"skipped\": \"", why.c_str(), why.empty() ? "" : "\"");
            ++totalBatches;
            ++perEntry[b.entry];
            if (vs.empty())
            {
                ++skipped;
                continue;
            }
            if (b.material >= 0)
                std::fprintf(obj, "usemtl m%d\n", b.material);
            for (const Vtx &x : vs)
            {
                std::fprintf(obj, "v %.4f %.4f %.4f %.3f %.3f %.3f\n", x.p[0], x.p[1], x.p[2],
                             std::fmin(1.0, x.c[0] / 255.0), std::fmin(1.0, x.c[1] / 255.0), std::fmin(1.0, x.c[2] / 255.0));
                std::fprintf(obj, "vt %.5f %.5f\n", x.st[0], 1.0f - x.st[1]);
            }
            const uint32_t type = prim & 7;
            const size_t n = vs.size();
            for (size_t i = 0; i + 2 < n; ++i)
            {
                size_t a, bb, cc;
                if (type == 3) // triangle list
                {
                    if (i % 3)
                        continue;
                    a = i, bb = i + 1, cc = i + 2;
                }
                else if (type == 5) // fan
                    a = 0, bb = i + 1, cc = i + 2;
                else // strip (4); billboards (entry 3) are strips of 4 corners too
                {
                    a = i, bb = i + 1, cc = i + 2;
                    if (i & 1)
                        std::swap(bb, cc);
                }
                std::fprintf(obj, "f %zu/%zu %zu/%zu %zu/%zu\n", vbase + a, vbase + a, vbase + bb, vbase + bb, vbase + cc, vbase + cc);
                ++totalTris;
            }
            vbase += n;
        }
        std::fprintf(js, "\n    ]}");
    }
    std::fprintf(js, "\n  ]\n}\n");
    std::fclose(obj);
    std::fclose(js);
    std::printf("%s: %zu chains, %zu batches (%zu skipped), %zu triangles;", stem.c_str(), model.chains.size(), totalBatches, skipped,
                totalTris);
    for (const auto &e : perEntry)
        std::printf(" entry%d x%zu", e.first, e.second);
    std::printf("\n");
    return 0;
}
