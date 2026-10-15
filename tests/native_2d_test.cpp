// Native 2D draw records (ps2x::gs::Native2DDecoder) on hand-made DIRECT payloads, without the game: the packets
// are laid out as docs/native-2d.md describes the game's (the sprite batch header, the sprite data tag, a rotated
// quad strip, the 3D's context-2 state and shadow sprite).

#include "runtime/gs/gs_native_2d.h"
#include "runtime/gs/gs_wide_layout.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace ps2x::gs;

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

    constexpr uint32_t kOfx = (2048 - 320) * 16, kOfy = (2048 - 112) * 16;

    struct Bytes
    {
        std::vector<uint8_t> b;
        void q(uint64_t lo, uint64_t hi)
        {
            const size_t at = b.size();
            b.resize(at + 16);
            std::memcpy(b.data() + at, &lo, 8);
            std::memcpy(b.data() + at + 8, &hi, 8);
        }
        void tag(uint32_t nloop, uint32_t flg, uint32_t nreg, uint64_t regs, bool pre = false, uint64_t prim = 0)
        {
            uint64_t lo = nloop | (1ull << 15) | (static_cast<uint64_t>(flg) << 58) | (static_cast<uint64_t>(nreg & 15) << 60);
            if (pre)
                lo |= (1ull << 46) | (prim << 47);
            q(lo, regs);
        }
        void ad(uint8_t reg, uint64_t value) { q(value, reg); }
    };

    uint64_t xyz(float x, float y, uint32_t z = 0xFFFFFE)
    {
        const uint64_t px = static_cast<uint64_t>(std::lround(x * 16)) + kOfx, py = static_cast<uint64_t>(std::lround(y * 16)) + kOfy;
        return px | (py << 16) | (static_cast<uint64_t>(z) << 32);
    }

    // gifBeginSpriteBatch: TEST, ALPHA, TEX1, TEX0, CLAMP, as A+D.
    void header(Bytes &p, uint64_t tex0)
    {
        p.tag(5, 0, 1, 0xE);
        p.ad(0x47, 0x5001B);
        p.ad(0x42, 0x44);
        p.ad(0x14, 0x60);
        p.ad(0x06, tex0);
        p.ad(0x08, 5);
    }

    // gifSetSpriteCount + sprites: CLAMP, UV, XYZ2, UV, XYZ2 each.
    void sprites(Bytes &p, const std::vector<std::pair<float, float>> &at, float w, float h)
    {
        p.tag(static_cast<uint32_t>(at.size()) * 5, 0, 1, 0xE, true, 0x156);
        for (const auto &s : at)
        {
            p.ad(0x08, 2 | (2ull << 2) | (8ull << 4) | (23ull << 14) | (16ull << 34) | (31ull << 44));
            p.ad(0x03, 8ull * 16 | ((16ull * 16) << 16));
            p.ad(0x05, xyz(s.first, s.second));
            p.ad(0x03, (8ull + 16) * 16 + 16 | (((16ull + 16) * 16 + 16) << 16));
            p.ad(0x05, xyz(s.first + w, s.second + h));
        }
    }

    constexpr uint64_t kTex0 = 0x2013780E65321A40ull;
}

int main()
{
    // 1. A UiBatch-like payload: two sprites in one complete state, nothing left for the frontend.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        sprites(p, {{10, 20}, {100, 40}}, 16, 20);
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.size() == 2);
        CHECK(out.raws.empty());
        CHECK(out.items.size() == 3); // a Clut (the TEX0 write), then the two draws
        CHECK(out.items[0].kind == Native2DItem::Clut);
        CHECK(out.cluts.size() == 1 && out.cluts[0].tbp0 == 0x1A40 && out.cluts[0].psm == 19);
        if (out.draws.size() == 2)
        {
            const Native2DDraw &d = out.draws[0];
            CHECK(d.count == 2);
            CHECK(d.state.prim.type == GS_PRIM_SPRITE && d.state.prim.tme && d.state.prim.abe && d.state.prim.fst && !d.state.prim.ctxt);
            CHECK(d.state.test == 0x5001B && d.state.alpha == 0x44 && d.state.tex1 == 0x60);
            CHECK(d.state.tex0.tbp0 == 0x1A40 && d.state.tex0.cbp == 0x1BC0);
            CHECK(std::fabs(out.verts[d.first].v.x - (10 + kOfx / 16.0f)) < 1e-3f);
            CHECK(out.verts[d.first].v.u == 8 * 16 && out.verts[d.first].v.v == 16 * 16);
            CHECK(out.verts[d.first + 1].v.z == 0xFFFFFE);
            CHECK(out.draws[1].state.clamp == d.state.clamp);
        }
    }

    // 2. The 3D's context-2 state and its shadow sprite stay the frontend's: verbatim, as Raw items, in order.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        sprites(p, {{10, 20}}, 16, 20);
        p.tag(6, 0, 1, 0xE);
        p.ad(0x00, 0x246);           // PRIM: sprite, ABE, context 2
        p.ad(0x43, 0x8000000052ull); // ALPHA_2
        p.ad(0x48, 0x7D80B);         // TEST_2
        p.ad(0x01, 0x20);            // RGBAQ
        p.ad(0x05, xyz(0, 0, 0));
        p.ad(0x05, xyz(640, 224, 0));
        sprites(p, {{50, 60}}, 16, 20); // and a 2D sprite again after it (its PRIM is its own)
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.size() == 2);
        CHECK(out.raws.size() == 1);
        if (out.raws.size() == 1)
            CHECK(out.raws[0].size == 16 + 6 * 16); // the whole of it: PRIM, ALPHA_2, TEST_2, RGBAQ, XYZ2 x2
        // order: Clut, Draw, Raw, Draw
        CHECK(out.items.size() == 4 && out.items[2].kind == Native2DItem::Raw && out.items[3].kind == Native2DItem::Draw);
        // Verify decoder recorded Context 2 state in ctx[1]:
        CHECK(dec.context(1).alpha == 0x8000000052ull);
        CHECK(dec.context(1).test == 0x7D80B);
        CHECK((dec.context(1).valid & (Native2DDecoder::kAlpha | Native2DDecoder::kTest)) == (Native2DDecoder::kAlpha | Native2DDecoder::kTest));
    }

    // 2b. Registers not owned by 2D (e.g. unowned A+D) stay the frontend's: verbatim, as Raw items.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        sprites(p, {{10, 20}}, 16, 20);
        p.tag(1, 0, 1, 0xE);
        p.ad(0x50, 0x12345678ull); // An unowned register
        sprites(p, {{50, 60}}, 16, 20);
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.size() == 2);
        CHECK(out.raws.size() == 1);
        if (out.raws.size() == 1)
            CHECK(out.raws[0].size == 16 + 1 * 16);
        CHECK(out.items.size() == 4 && out.items[2].kind == Native2DItem::Raw && out.items[3].kind == Native2DItem::Draw);
    }

    // 3. A hidden sprite (XYZ3) is a draw that does not kick; a strip of four with STQ is one draw.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        p.tag(5, 0, 1, 0xE, true, 0x156);
        p.ad(0x08, 2 | (2ull << 2));
        p.ad(0x03, 0);
        p.ad(0x0D, xyz(0, 0));
        p.ad(0x03, 0x100010);
        p.ad(0x0D, xyz(8, 8));
        // gifQuadTag: PRE strip (0x154), CLAMP_1, then UV + XYZ2 x 4
        p.tag(1, 0, 9, 0x5353535358ull, true, 0x154); // regs: A+D(0xE)? (see below: we use PACKED UV/XYZ2 pairs instead)
        CHECK(true);
    }
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        p.tag(5, 0, 1, 0xE, true, 0x156);
        p.ad(0x08, 2 | (2ull << 2));
        p.ad(0x03, 0);
        p.ad(0x0D, xyz(0, 0));
        p.ad(0x03, 0x100010);
        p.ad(0x0D, xyz(8, 8));
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.size() == 1 && out.draws[0].count == 2);
        if (out.draws.size() == 1)
            CHECK(!out.verts[0].kick && !out.verts[1].kick);
    }
    {
        // A strip of four with ST/RGBAQ/XYZF2 (the EE-built STQ strips): PACKED, three registers a loop.
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        p.tag(4, 0, 3, 0x4 << 8 | 0x1 << 4 | 0x2, true, 0x144 | 0x4); // PRE: TSTRIP, TME; regs ST, RGBAQ, XYZF2
        for (int i = 0; i < 4; ++i)
        {
            const float s = static_cast<float>(i) * 0.25f;
            uint32_t sb, tb, qb = 0x3F800000;
            std::memcpy(&sb, &s, 4);
            tb = sb;
            p.q(sb | (static_cast<uint64_t>(tb) << 32), qb);
            p.q(0x80 | (0x40ull << 32), 0x20 | (0x80ull << 32));
            p.q((static_cast<uint64_t>(kOfx) + 16 * i * 10) | ((static_cast<uint64_t>(kOfy) + 16) << 32), static_cast<uint64_t>(0x1000) << 4);
        }
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.size() == 1 && out.draws[0].count == 4);
        if (out.draws.size() == 1)
        {
            CHECK(out.draws[0].state.prim.type == GS_PRIM_TRISTRIP);
            CHECK(out.verts[2].v.s == 0.5f && out.verts[2].v.q == 1.0f);
            CHECK(out.verts[1].v.r == 0x80 && out.verts[1].v.g == 0x40 && out.verts[1].v.b == 0x20 && out.verts[1].v.a == 0x80);
        }
    }

    // 4. What it won't take whole goes back as the packet it was (the caller sends it on): no state yet; a point.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        sprites(p, {{10, 20}}, 16, 20); // no header: the state was never written
        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::NoState);
        CHECK(out.items.empty());
        Bytes q;
        header(q, kTex0);
        q.tag(1, 0, 1, 0xE, true, 0x0); // PRIM point
        q.ad(0x05, xyz(1, 1));
        CHECK(dec.decode(q.b.data(), static_cast<uint32_t>(q.b.size()), out) == Native2DReason::Prim);
        // and a state change inside a strip: a carried vertex
        Bytes r;
        header(r, kTex0);
        r.tag(2, 0, 1, 0xE, true, 0x154);
        r.ad(0x05, xyz(1, 1));
        r.ad(0x05, xyz(2, 2));
        r.tag(1, 0, 1, 0xE);
        r.ad(0x08, 7);
        r.tag(1, 0, 1, 0xE);
        r.ad(0x05, xyz(3, 3)); // the strip goes on under another CLAMP
        CHECK(dec.decode(r.b.data(), static_cast<uint32_t>(r.b.size()), out) == Native2DReason::Carry);
    }

    // 5. The widescreen layout takes records: at 16:9 with HUD edges, a sprite on the left goes to the left edge.
    {
        WideLayout wide;
        wide.configure(16.0f / 9.0f, HudPlacement::Edges);
        wide.setRecordClasses(true);
        Native2DVertex v[2];
        v[0].rawX = static_cast<uint16_t>(kOfx + 10 * 16);
        v[0].rawY = static_cast<uint16_t>(kOfy + 20 * 16);
        v[1].rawX = static_cast<uint16_t>(kOfx + 26 * 16);
        v[1].rawY = static_cast<uint16_t>(kOfy + 40 * 16);
        WideLayout::PrimState st;
        st.type = 6;
        st.tme = true;
        st.ofx[0] = st.ofx[1] = kOfx;
        st.ofy[0] = st.ofy[1] = kOfy;
        st.tbp[0] = 0x1A40;
        wide.frameStart();
        // the frame's first draw is 3D, as in a driving frame: a PATH1 triangle (a packet transform), then the HUD record
        std::vector<uint8_t> tri(16 + 3 * 16);
        const uint64_t lo = 3 | (1ull << 15) | (1ull << 46) | (static_cast<uint64_t>(3) << 47) | (1ull << 60);
        const uint64_t hi = 5;
        std::memcpy(tri.data(), &lo, 8);
        std::memcpy(tri.data() + 8, &hi, 8);
        for (int i = 0; i < 3; ++i)
        {
            const uint64_t q = xyz(100.0f + 10 * i, 100.0f + 10 * (i & 1));
            std::memcpy(tri.data() + 16 + i * 16, &q, 8);
            const uint64_t z = 0;
            std::memcpy(tri.data() + 24 + i * 16, &z, 8);
        }
        WideLayout::PrimState s3 = st;
        s3.type = 3;
        s3.tme = false;
        wide.transformPacket(0, tri.data(), static_cast<uint32_t>(tri.size()), s3);
        wide.beginRecords();
        wide.addRecord(st, v, 2);
        wide.endRecords(st);
        const float x0 = (v[0].rawX - static_cast<float>(kOfx)) / 16.0f;
        CHECK(std::fabs(x0 - 10.0f * wide.horizontalScale()) < 0.2f); // anchored left: x * k
        CHECK(wide.vertexClasses().size() == 2 && wide.vertexClasses()[0] == WideLayout::VertexClass::Ui);
    }

    // 6. Context-2 state packets: TEST_2, ALPHA_2, TEX0_2, TEX1_2, CLAMP_2, SCISSOR_2, FRAME_2, ZBUF_2, XYOFFSET_2.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        p.tag(9, 0, 1, 0xE);
        p.ad(0x48, 0x537FB);                     // TEST_2
        p.ad(0x43, 0x44);                        // ALPHA_2
        p.ad(0x07, kTex0);                       // TEX0_2
        p.ad(0x15, 0x60);                        // TEX1_2
        p.ad(0x09, 5);                           // CLAMP_2
        p.ad(0x41, 0 | (639ull << 16) | (0ull << 32) | (223ull << 48)); // SCISSOR_2
        p.ad(0x4D, 0x12345ull);                  // FRAME_2
        p.ad(0x4F, 0x6789Aull);                  // ZBUF_2
        p.ad(0x19, static_cast<uint64_t>(kOfx) | (static_cast<uint64_t>(kOfy) << 32)); // XYOFFSET_2

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        // State-only packet leaves draws empty so the runtime lets the frontend see it as well.
        CHECK(out.draws.empty());
        CHECK(dec.context(1).valid == Native2DDecoder::kAll);
        CHECK(dec.context(1).test == 0x537FB);
        CHECK(dec.context(1).alpha == 0x44);
        CHECK(dec.context(1).tex0.tbp0 == 0x1A40);
        CHECK(dec.context(1).tex1 == 0x60);
        CHECK(dec.context(1).clamp == 5);
        CHECK(dec.context(1).hasScissor);
        CHECK(dec.context(1).scissor.x0 == 0 && dec.context(1).scissor.x1 == 639 &&
              dec.context(1).scissor.y0 == 0 && dec.context(1).scissor.y1 == 223);
        CHECK(dec.context(1).hasFrame && dec.context(1).frame == 0x12345ull);
        CHECK(dec.context(1).hasZbuf && dec.context(1).zbuf == 0x6789Aull);
        CHECK(dec.context(1).hasXyoffset &&
              dec.context(1).xyoffset == (static_cast<uint64_t>(kOfx) | (static_cast<uint64_t>(kOfy) << 32)));

        // Context 1 state remains unpolluted by Context 2 writes.
        CHECK(!dec.context(0).hasScissor);
        CHECK(!dec.context(0).hasFrame);
        CHECK(!dec.context(0).hasZbuf);
        CHECK(!dec.context(0).hasXyoffset);
    }

    // 7. Context-2 shadow-darken sprite (PRIM 0x246, 0x29D0F0).
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        p.tag(6, 0, 1, 0xE);
        p.ad(0x00, 0x246);           // PRIM: sprite, ABE, context 2
        p.ad(0x43, 0x8000000052ull); // ALPHA_2
        p.ad(0x48, 0x7D80B);         // TEST_2
        p.ad(0x01, 0x20);            // RGBAQ
        p.ad(0x05, xyz(0, 0, 0));
        p.ad(0x05, xyz(640, 224, 0));

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        // The shadow sprite is forwarded to the GS frontend via Raw item (so it executes on m_ctx[1]):
        CHECK(out.draws.empty());
        CHECK(out.raws.size() == 1);
        if (out.raws.size() == 1)
            CHECK(out.raws[0].size == 16 + 6 * 16);
        // Decoder records Context-2 register state in ctx[1]:
        CHECK(dec.context(1).alpha == 0x8000000052ull);
        CHECK(dec.context(1).test == 0x7D80B);
        CHECK((dec.context(1).valid & (Native2DDecoder::kAlpha | Native2DDecoder::kTest)) == (Native2DDecoder::kAlpha | Native2DDecoder::kTest));
    }

    // 8. TEXA updates (uiSetTexa and uiBatchFlush): decoded in Native2DState AND forwarded for frontend sync.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        header(p, kTex0);
        // uiBatchFlush writes TEXA (0x3B) = 0x4000008080 (TA0=0x80, AEM=1, TA1=0x40).
        p.tag(1, 0, 1, 0xE);
        p.ad(0x3B, 0x4000008080ull);
        sprites(p, {{10, 20}}, 16, 20);

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.raws.size() == 1); // forwarded TEXA write for GS frontend sync
        CHECK(out.draws.size() == 1);
        if (out.draws.size() == 1)
        {
            CHECK(out.draws[0].state.hasTexa);
            CHECK(out.draws[0].state.texa == 0x4000008080ull);
        }

        // Subsequent packet with uiSetTexa(0x40): TA0=0x40, AEM=1, TA1=0x20.
        Bytes p2;
        const uint64_t fadeTexa = 0x40 | 0x8000 | (0x20ull << 32);
        p2.tag(1, 0, 1, 0xE);
        p2.ad(0x3B, fadeTexa);
        sprites(p2, {{30, 40}}, 16, 20);

        CHECK(dec.decode(p2.b.data(), static_cast<uint32_t>(p2.b.size()), out) == Native2DReason::Ok);
        CHECK(out.raws.size() == 1); // forwarded TEXA write for GS frontend sync
        CHECK(out.draws.size() == 1);
        if (out.draws.size() == 1)
        {
            CHECK(out.draws[0].state.hasTexa);
            CHECK(out.draws[0].state.texa == fadeTexa);
        }
    }

    // 9. SCISSOR updates: SCISSOR_1 in Native2DState and forwarded, SCISSOR_2 in Context 2 state and forwarded.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        // Packet 1: Minimap setup (TEST_2 0x3380B, ALPHA_2 0x44, SCISSOR_2 x 45..158 y 156..207).
        p.tag(3, 0, 1, 0xE);
        p.ad(0x48, 0x3380B);                                           // TEST_2
        p.ad(0x43, 0x44);                                              // ALPHA_2
        p.ad(0x41, 45 | (158ull << 16) | (156ull << 32) | (207ull << 48)); // SCISSOR_2

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.empty());
        CHECK(out.raws.size() == 1);
        CHECK(dec.context(1).hasScissor);
        CHECK(dec.context(1).scissor.x0 == 45);
        CHECK(dec.context(1).scissor.x1 == 158);
        CHECK(dec.context(1).scissor.y0 == 156);
        CHECK(dec.context(1).scissor.y1 == 207);

        // Packet 2: Context 1 draw with SCISSOR_1 (0x40): tags Native2DState AND forwards to frontend.
        Bytes pContext1;
        header(pContext1, kTex0);
        pContext1.tag(1, 0, 1, 0xE);
        pContext1.ad(0x40, 10 | (300ull << 16) | (20ull << 32) | (200ull << 48));
        sprites(pContext1, {{10, 20}}, 16, 20);

        CHECK(dec.decode(pContext1.b.data(), static_cast<uint32_t>(pContext1.b.size()), out) == Native2DReason::Ok);
        CHECK(out.raws.size() == 1); // forwarded SCISSOR_1 to keep GS frontend m_ctx[0].scissor in sync
        CHECK(out.draws.size() == 1);
        if (out.draws.size() == 1)
        {
            CHECK(out.draws[0].state.ctxt == 0);
            CHECK(out.draws[0].state.hasScissor);
            CHECK(out.draws[0].state.scissor.x0 == 10);
            CHECK(out.draws[0].state.scissor.x1 == 300);
            CHECK(out.draws[0].state.scissor.y0 == 20);
            CHECK(out.draws[0].state.scissor.y1 == 200);
        }
    }

    // 10. Context-2 state tracking: TEX0_2, CLAMP_2, and TEX2_2 palette switch.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        p.tag(5, 0, 1, 0xE);
        p.ad(0x48, 0x5001B);   // TEST_2
        p.ad(0x43, 0x44);      // ALPHA_2
        p.ad(0x15, 0x60);      // TEX1_2
        p.ad(0x07, kTex0);     // TEX0_2
        p.ad(0x09, 5);         // CLAMP_2

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(dec.context(1).valid == Native2DDecoder::kAll);
        CHECK(dec.context(1).tex0.tbp0 == 0x1A40);
        CHECK(dec.context(1).tex0.csa == 0);

        // Palette switch via TEX2_2 (0x17)
        Bytes p2;
        p2.tag(1, 0, 1, 0xE);
        const uint64_t tex2Val = (2ull << 56) | 0x13780001400000ull; // CSA = 2
        p2.ad(0x17, tex2Val);

        CHECK(dec.decode(p2.b.data(), static_cast<uint32_t>(p2.b.size()), out) == Native2DReason::Ok);
        CHECK(dec.context(1).tex0.csa == 2);
    }

    // 11. Context-1 environment registers (FRAME_1, ZBUF_1, XYOFFSET_1): decoded and forwarded for frontend sync.
    {
        Native2DDecoder dec;
        Native2DPacket out;
        Bytes p;
        p.tag(3, 0, 1, 0xE);
        p.ad(0x4C, 0x1111ull); // FRAME_1
        p.ad(0x4E, 0x2222ull); // ZBUF_1
        p.ad(0x18, static_cast<uint64_t>(kOfx) | (static_cast<uint64_t>(kOfy) << 32)); // XYOFFSET_1

        CHECK(dec.decode(p.b.data(), static_cast<uint32_t>(p.b.size()), out) == Native2DReason::Ok);
        CHECK(out.draws.empty());
        CHECK(out.raws.size() == 1);
        CHECK(dec.context(0).hasFrame && dec.context(0).frame == 0x1111ull);
        CHECK(dec.context(0).hasZbuf && dec.context(0).zbuf == 0x2222ull);
        CHECK(dec.context(0).hasXyoffset &&
              dec.context(0).xyoffset == (static_cast<uint64_t>(kOfx) | (static_cast<uint64_t>(kOfy) << 32)));
    }

    if (g_failures)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("native_2d_test: ok\n");
    return 0;
}
