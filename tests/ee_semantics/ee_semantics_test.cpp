// Differential tests of the recompiled R5900 code: each case is an instruction (or a small
// program) translated by the real recompiler (ee_semantics_gen, included below as a generated
// file), run on a guest context in the EE's float mode, and compared with a reference model written
// here from the EE Core / VU manuals (integer soft-float with round-toward-zero, flush-to-zero and
// overflow-to-MAX; byte-wise models of the unaligned loads and stores; lane models of the MMI ops).
//
// What it protects: the float mode of the executor thread (ps2_ee_fpu_mode.h), the 64-bit sign
// tests of branches, the unaligned and quadword memory ops, HI/LO and HI1/LO1, the MMI ops the
// game uses, and VU0 macro-mode arithmetic.

#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

// The few runtime entry points the generated code names. Nothing here is reached by a passing
// case; a case that reaches one fails loudly.
static int g_unexpected = 0;
bool PS2Runtime::dispatchGuestBranch(uint8_t *, R5900Context *ctx, uint32_t targetPc, uint32_t, uint32_t,
                                     GuestBranchKind, const char *)
{
    ctx->pc = targetPc;
    return false; // a transfer out of the function under test: the program ends here
}
void PS2Runtime::SignalException(R5900Context *, PS2Exception) { ++g_unexpected; }
void PS2Runtime::handleTrap(uint8_t *, R5900Context *) { ++g_unexpected; }
void PS2Runtime::handleBreak(uint8_t *, R5900Context *) { ++g_unexpected; }
void PS2Runtime::handleSyscall(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; }
// eeCheckpointDue is inline now (budget 0 in the zeroed stand-in below sends it here).
bool PS2Runtime::eeCheckpointDueSlow(uint32_t) noexcept { return false; }
uint8_t PS2Runtime::Load8(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; return 0; }
uint16_t PS2Runtime::Load16(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; return 0; }
uint32_t PS2Runtime::Load32(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; return 0; }
uint64_t PS2Runtime::Load64(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; return 0; }
__m128i PS2Runtime::Load128(uint8_t *, R5900Context *, uint32_t) { ++g_unexpected; return _mm_setzero_si128(); }
void PS2Runtime::Store8(uint8_t *, R5900Context *, uint32_t, uint8_t) { ++g_unexpected; }
void PS2Runtime::Store16(uint8_t *, R5900Context *, uint32_t, uint16_t) { ++g_unexpected; }
void PS2Runtime::Store32(uint8_t *, R5900Context *, uint32_t, uint32_t) { ++g_unexpected; }
void PS2Runtime::Store64(uint8_t *, R5900Context *, uint32_t, uint64_t) { ++g_unexpected; }
void PS2Runtime::Store128(uint8_t *, R5900Context *, uint32_t, __m128i) { ++g_unexpected; }

#include "ee_semantics_cases.inc"

namespace
{
    int g_checks = 0;
    int g_failed = 0;

    uint8_t *g_rdram = nullptr;
    // Zeroed storage: only the inline checkpoint reads it (budget 0 -> eeCheckpointDueSlow, above).
    alignas(64) unsigned char g_runtimeStorage[sizeof(PS2Runtime)];
    PS2Runtime *g_runtime = reinterpret_cast<PS2Runtime *>(g_runtimeStorage);

    void fail(const char *what, const char *name, const std::string &detail)
    {
        ++g_failed;
        static std::map<std::string, int> perCase;
        const int n = ++perCase[std::string(what) + " " + name];
        if (n <= 3)
            std::printf("FAIL %s [%s] %s\n", what, name, detail.c_str());
        else if (n == 4)
            std::printf("FAIL %s [%s] ... more of the same\n", what, name);
    }

    std::string hex(uint64_t v)
    {
        char b[32];
        std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(v));
        return b;
    }

    template <typename T>
    void expectEq(const char *what, const char *name, T got, T want, const std::string &ctx = "")
    {
        ++g_checks;
        if (got != want)
            fail(what, name, "got " + hex(static_cast<uint64_t>(got)) + " want " + hex(static_cast<uint64_t>(want)) + " " + ctx);
    }

    uint64_t rng = 0x9E3779B97F4A7C15ull;
    uint64_t next()
    {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    }

    // ---------------------------------------------------------------- the context
    struct Ctx
    {
        R5900Context *c;
        Ctx() : c(new R5900Context()) {}
        ~Ctx() { delete c; }
        void gpr(int i, uint64_t lo, uint64_t hi = 0) { c->r[i] = _mm_set_epi64x(static_cast<long long>(hi), static_cast<long long>(lo)); }
        uint64_t lo(int i) const { return static_cast<uint64_t>(_mm_cvtsi128_si64(c->r[i])); }
        uint64_t hi(int i) const { return static_cast<uint64_t>(_mm_extract_epi64(c->r[i], 1)); }
        void setF(int i, uint32_t bits) { std::memcpy(&c->f[i], &bits, 4); }
        uint32_t getF(int i) const { uint32_t b; std::memcpy(&b, &c->f[i], 4); return b; }
    };

    using Fn = void (*)(uint8_t *, R5900Context *, PS2Runtime *);

    Fn inst(const char *name)
    {
        for (const auto &c : kInstCases)
            if (std::strcmp(c.name, name) == 0)
                return c.fn;
        std::printf("FAIL missing case %s\n", name);
        ++g_failed;
        return [](uint8_t *, R5900Context *, PS2Runtime *) {};
    }
    Fn program(const char *name)
    {
        for (const auto &c : kProgramCases)
            if (std::strcmp(c.name, name) == 0)
                return c.fn;
        std::printf("FAIL missing program %s\n", name);
        ++g_failed;
        return [](uint8_t *, R5900Context *, PS2Runtime *) {};
    }

    // ---------------------------------------------------------------- soft float, EE rules
    // value = m * 2^e (m up to 24 bits); zero when m == 0. Denormal operands are zero.
    struct SF
    {
        bool neg;
        int e;
        uint64_t m;
    };
    SF dec(uint32_t b)
    {
        SF s;
        s.neg = (b >> 31) != 0;
        const uint32_t ex = (b >> 23) & 0xFF;
        const uint32_t man = b & 0x7FFFFF;
        if (ex == 0)
        {
            s.m = 0;
            s.e = 0;
        }
        else
        {
            s.m = man | 0x800000u;
            s.e = static_cast<int>(ex) - 150;
        }
        return s;
    }
    using u128 = unsigned __int128;
    int msb(u128 v)
    {
        int p = -1;
        while (v)
        {
            ++p;
            v >>= 1;
        }
        return p;
    }
    // Encode neg * (M * 2^E) truncated toward zero (or, with `nearest`, rounded to nearest: M is
    // exact or a floor of the true value with ties impossible, so the bit below the mantissa decides);
    // overflow gives MAX, underflow zero.
    uint32_t enc(bool neg, u128 M, int E, bool nearest = false)
    {
        const uint32_t sign = neg ? 0x80000000u : 0u;
        if (M == 0)
            return sign;
        const int p = msb(M);
        uint32_t mant;
        int ne;
        if (p >= 23)
        {
            mant = static_cast<uint32_t>(M >> (p - 23));
            ne = E + (p - 23);
        }
        else
        {
            mant = static_cast<uint32_t>(M << (23 - p));
            ne = E - (23 - p);
        }
        int biased = ne + 150;
        if (nearest)
        {
            const int sh = p - 24; // the bit below the mantissa
            const bool up = sh >= 0 ? (((M >> sh) & 1) != 0) : false;
            if (up)
            {
                ++mant;
                if (mant == 0x1000000u)
                {
                    mant = 0x800000u;
                    ++biased;
                }
            }
        }
        if (biased >= 255)
            return sign | 0x7F7FFFFFu;
        if (biased <= 0)
            return sign;
        return sign | (static_cast<uint32_t>(biased) << 23) | (mant & 0x7FFFFFu);
    }
    uint32_t rAdd(uint32_t a, uint32_t b, bool sub = false)
    {
        SF x = dec(a), y = dec(b);
        if (sub)
            y.neg = !y.neg;
        if (x.m == 0 && y.m == 0)
            return (x.neg && y.neg) ? 0x80000000u : 0u; // -0 + -0; +0 otherwise (round toward zero)
        if (x.m == 0)
            return enc(y.neg, y.m, y.e);
        if (y.m == 0)
            return enc(x.neg, x.m, x.e);
        // Make x the operand with the larger exponent.
        if (x.e < y.e)
            std::swap(x, y);
        const int diff = x.e - y.e;
        const int kShift = 90;
        u128 L = static_cast<u128>(x.m) << kShift; // x * 2^(x.e - kShift)
        u128 S;
        bool stickyOnly = false;
        if (diff <= kShift)
            S = static_cast<u128>(y.m) << (kShift - diff);
        else
        {
            S = 1; // below the lsb of L: a sticky amount
            stickyOnly = true;
        }
        const int E = x.e - kShift;
        if (x.neg == y.neg)
            return enc(x.neg, stickyOnly ? L : L + S, E);
        // Opposite signs: |x| - |y| (or the reverse when y is larger).
        if (!stickyOnly && S > L)
            return enc(y.neg, S - L, E);
        if (!stickyOnly && S == L)
            return 0u;
        return enc(x.neg, L - S, E);
    }
    uint32_t rMul(uint32_t a, uint32_t b)
    {
        const SF x = dec(a), y = dec(b);
        const bool neg = x.neg != y.neg;
        if (x.m == 0 || y.m == 0)
            return neg ? 0x80000000u : 0u;
        return enc(neg, static_cast<u128>(x.m) * y.m, x.e + y.e);
    }
    uint32_t rDiv(uint32_t a, uint32_t b, bool nearest = false)
    {
        const SF x = dec(a), y = dec(b);
        const bool neg = x.neg != y.neg;
        if (y.m == 0)
            return (neg ? 0x80000000u : 0u) | 0x7F7FFFFFu;
        if (x.m == 0)
            return neg ? 0x80000000u : 0u;
        const u128 q = (static_cast<u128>(x.m) << 80) / y.m;
        return enc(neg, q, x.e - y.e - 80, nearest);
    }
    u128 isqrt(u128 n)
    {
        u128 lo = 0, hi = static_cast<u128>(1) << 60;
        while (lo < hi)
        {
            const u128 mid = (lo + hi + 1) >> 1;
            if (mid * mid <= n)
                lo = mid;
            else
                hi = mid - 1;
        }
        return lo;
    }
    uint32_t rSqrtAbs(uint32_t b, bool nearest = false)
    {
        SF y = dec(b);
        if (y.m == 0)
            return b & 0x80000000u;
        u128 M = y.m;
        int e = y.e;
        if (e & 1)
        {
            M <<= 1;
            e -= 1;
        }
        const int k = 46;
        M <<= (2 * k);
        e -= 2 * k;
        return enc(false, isqrt(M), e / 2, nearest);
    }
    uint32_t rCvtW(uint32_t b)
    {
        const SF x = dec(b);
        if (x.m == 0)
            return 0;
        int64_t v;
        if (x.e >= 0)
        {
            if (x.e > 8)
                return x.neg ? 0x80000000u : 0x7FFFFFFFu;
            v = static_cast<int64_t>(x.m) << x.e;
        }
        else
            v = (-x.e >= 63) ? 0 : static_cast<int64_t>(x.m >> (-x.e));
        if (v > 0x7FFFFFFFll)
            return x.neg ? 0x80000000u : 0x7FFFFFFFu;
        return static_cast<uint32_t>(x.neg ? -v : v);
    }
    uint32_t rCvtS(int32_t v)
    {
        if (v == 0)
            return 0;
        const bool neg = v < 0;
        const uint64_t a = neg ? static_cast<uint64_t>(-static_cast<int64_t>(v)) : static_cast<uint64_t>(v);
        return enc(neg, a, 0);
    }
    // ITOF/FTOI with the VU's fixed point (frac bits): int / 2^frac, float * 2^frac truncated.
    uint32_t rItof(int32_t v, int frac)
    {
        if (v == 0)
            return 0;
        const bool neg = v < 0;
        const uint64_t a = neg ? static_cast<uint64_t>(-static_cast<int64_t>(v)) : static_cast<uint64_t>(v);
        return enc(neg, a, -frac);
    }
    uint32_t rFtoi(uint32_t b, int frac)
    {
        const SF x = dec(b);
        if (x.m == 0)
            return 0;
        const int e = x.e + frac;
        int64_t v;
        if (e >= 0)
        {
            if (e > 8)
                return x.neg ? 0x80000000u : 0x7FFFFFFFu;
            v = static_cast<int64_t>(x.m) << e;
        }
        else
            v = (-e >= 63) ? 0 : static_cast<int64_t>(x.m >> (-e));
        if (v > 0x7FFFFFFFll)
            return x.neg ? 0x80000000u : 0x7FFFFFFFu;
        return static_cast<uint32_t>(x.neg ? -v : v);
    }
    bool isZeroBits(uint32_t b) { return ((b >> 23) & 0xFF) == 0; }
    bool sameFloat(uint32_t got, uint32_t want)
    {
        // A zero result may carry either sign.
        if (isZeroBits(got) && isZeroBits(want) && (got & 0x7FFFFFu) == 0 && (want & 0x7FFFFFu) == 0)
            return true;
        return got == want;
    }

    // Operands: normal numbers over the whole range, zeros, denormals (flushed), near MAX / MIN.
    uint32_t operand()
    {
        const uint64_t r = next();
        const uint32_t sign = (r & 1) ? 0x80000000u : 0u;
        switch ((r >> 1) % 12)
        {
        case 0: return sign;                                              // +-0
        case 1: return sign | static_cast<uint32_t>((r >> 8) & 0x7FFFFF); // denormal
        case 2: return sign | 0x7F7FFFFFu;                                // MAX
        case 3: return sign | (0x7F000000u + static_cast<uint32_t>((r >> 8) & 0x7FFFFF));
        case 4: return sign | 0x00800000u | static_cast<uint32_t>((r >> 8) & 0x7FFFFF); // smallest normals
        case 5: return sign | 0x3F800000u | static_cast<uint32_t>((r >> 8) & 0x7FFFFF); // 1..2
        case 6: return sign | (static_cast<uint32_t>(((r >> 8) % 20) + 118) << 23) | static_cast<uint32_t>((r >> 40) & 0x7FFFFF);
        default:
        {
            uint32_t ex = static_cast<uint32_t>((r >> 8) % 254) + 1; // 1..254, never 255
            return sign | (ex << 23) | static_cast<uint32_t>((r >> 40) & 0x7FFFFF);
        }
        }
    }
    uint32_t operandNear(uint32_t other)
    {
        // Similar magnitude (cancellation) or a tiny addend, to exercise truncation.
        const uint64_t r = next();
        if (r % 3 == 0)
            return other ^ 0x80000000u ^ static_cast<uint32_t>((r >> 8) & 7);
        if (r % 3 == 1)
            return (other & 0x80000000u) | ((((other >> 23) & 0xFF) > 40 ? ((other >> 23) & 0xFF) - 30 : 1u) << 23) | static_cast<uint32_t>((r >> 8) & 0x7FFFFF);
        return operand();
    }

    // ---------------------------------------------------------------- tests
    void testFloatMode()
    {
        ++g_checks;
        if (!ps2_ee_fpu::threadInEeMode())
            fail("float mode", "thread", "the thread is not in round-toward-zero, flush-to-zero mode");
    }

    void testFpu()
    {
        Ctx t;
        const Fn add = inst("fadd"), sub = inst("fsub"), mul = inst("fmul"), div = inst("fdiv");
        for (int i = 0; i < 40000; ++i)
        {
            const uint32_t a = operand();
            const uint32_t b = (i % 3 == 0) ? operandNear(a) : operand();
            t.setF(0, a);
            t.setF(1, b);
            struct { const char *n; Fn fn; uint32_t want; } ops[] = {
                {"fadd", add, rAdd(a, b)}, {"fsub", sub, rAdd(a, b, true)}, {"fmul", mul, rMul(a, b)}, {"fdiv", div, rDiv(a, b, true)}};
            for (auto &o : ops)
            {
                t.setF(2, 0x12345678u);
                o.fn(g_rdram, t.c, g_runtime);
                ++g_checks;
                if (!sameFloat(t.getF(2), o.want))
                    fail(o.n, "random", "a=" + hex(a) + " b=" + hex(b) + " got " + hex(t.getF(2)) + " want " + hex(o.want));
            }
        }
        // sqrt / rsqrt / cvt
        const Fn sq = inst("fsqrt"), rs = inst("frsqrt"), cw = inst("fcvt_w"), cs = inst("fcvt_s");
        for (int i = 0; i < 20000; ++i)
        {
            const uint32_t a = operand(), b = operand();
            t.setF(0, a);
            t.setF(1, b);
            sq(g_rdram, t.c, g_runtime);
            ++g_checks;
            if (!sameFloat(t.getF(2), rSqrtAbs(b, true)))
                fail("fsqrt", "random", "b=" + hex(b) + " got " + hex(t.getF(2)) + " want " + hex(rSqrtAbs(b, true)));
            t.setF(0, a);
            t.setF(1, b);
            rs(g_rdram, t.c, g_runtime);
            if (!isZeroBits(b))
            {
                const uint32_t want = rDiv(a, rSqrtAbs(b, true), true);
                ++g_checks;
                if (!sameFloat(t.getF(2), want))
                    fail("frsqrt", "random", "a=" + hex(a) + " b=" + hex(b) + " got " + hex(t.getF(2)) + " want " + hex(want));
            }
            else
                expectEq("frsqrt by zero", "special", t.getF(2) & 0x7FFFFFFFu, 0x7F7FFFFFu);
            t.setF(0, a);
            cw(g_rdram, t.c, g_runtime);
            expectEq("fcvt_w", "random", t.getF(2), rCvtW(a), "a=" + hex(a));
            const int32_t iv = static_cast<int32_t>(next());
            std::memcpy(&t.c->f[0], &iv, 4);
            cs(g_rdram, t.c, g_runtime);
            expectEq("fcvt_s", "random", t.getF(2), rCvtS(iv), "v=" + hex(static_cast<uint32_t>(iv)));
        }
        // CVT.S.W of 16777219 truncates to 16777218 (round-to-nearest would give 16777220)
        {
            int32_t v = 16777219;
            std::memcpy(&t.c->f[0], &v, 4);
            cs(g_rdram, t.c, g_runtime);
            expectEq("cvt.s.w truncates", "16777219", t.getF(2), 0x4B800001u);
        }
        // Overflow gives +-MAX, underflow gives zero, denormal operands are zero.
        t.setF(0, 0x7F7FFFFFu);
        t.setF(1, 0x7F7FFFFFu);
        add(g_rdram, t.c, g_runtime);
        expectEq("overflow add", "max+max", t.getF(2), 0x7F7FFFFFu);
        mul(g_rdram, t.c, g_runtime);
        expectEq("overflow mul", "max*max", t.getF(2), 0x7F7FFFFFu);
        t.setF(0, 0xFF7FFFFFu);
        mul(g_rdram, t.c, g_runtime);
        expectEq("overflow mul", "-max*max", t.getF(2), 0xFF7FFFFFu);
        t.setF(0, 0x00800000u);
        t.setF(1, 0x3F000000u);
        mul(g_rdram, t.c, g_runtime);
        ++g_checks;
        if ((t.getF(2) & 0x7FFFFFFFu) != 0)
            fail("underflow mul", "min*0.5", "result " + hex(t.getF(2)) + " is not zero");
        t.setF(0, 0x00400000u); // denormal: an operand of zero, so 0 + 2.0 = 2.0 exactly
        t.setF(1, 0x40000000u);
        add(g_rdram, t.c, g_runtime);
        expectEq("denormal operand", "add", t.getF(2), 0x40000000u);
        t.setF(0, 0x00400000u);
        t.setF(1, 0x00400001u);
        mul(g_rdram, t.c, g_runtime);
        ++g_checks;
        if ((t.getF(2) & 0x7FFFFFFFu) != 0)
            fail("denormal operand", "mul", "result " + hex(t.getF(2)) + " is not zero");
        // Compare flags
        const Fn ceq = inst("fceq"), clt = inst("fclt"), cle = inst("fcle");
        const float vals[] = {-2.0f, -0.0f, 0.0f, 1.0f, 1.0f, 3.5f};
        for (float x : vals)
            for (float y : vals)
            {
                std::memcpy(&t.c->f[0], &x, 4);
                std::memcpy(&t.c->f[1], &y, 4);
                t.c->fcr31 = 0;
                ceq(g_rdram, t.c, g_runtime);
                expectEq("c.eq", "fcr31", (t.c->fcr31 >> 23) & 1u, x == y ? 1u : 0u);
                t.c->fcr31 = 0x800000;
                clt(g_rdram, t.c, g_runtime);
                expectEq("c.lt", "fcr31", (t.c->fcr31 >> 23) & 1u, x < y ? 1u : 0u);
                cle(g_rdram, t.c, g_runtime);
                expectEq("c.le", "fcr31", (t.c->fcr31 >> 23) & 1u, x <= y ? 1u : 0u);
            }
        // MADD.S has no fused multiply-add: ACC + fs*ft rounds the product first.
        const Fn madd = inst("fmadd"), mula = inst("fmula");
        for (int i = 0; i < 20000; ++i)
        {
            const uint32_t acc = operand(), a = operand(), b = operand();
            t.c->f_acc = 0;
            std::memcpy(&t.c->f_acc, &acc, 4);
            t.setF(0, a);
            t.setF(1, b);
            madd(g_rdram, t.c, g_runtime);
            const uint32_t want = rAdd(acc, rMul(a, b));
            ++g_checks;
            if (!sameFloat(t.getF(2), want))
                fail("madd.s", "random", "acc=" + hex(acc) + " a=" + hex(a) + " b=" + hex(b) + " got " + hex(t.getF(2)) + " want " + hex(want));
            mula(g_rdram, t.c, g_runtime);
            uint32_t got;
            std::memcpy(&got, &t.c->f_acc, 4);
            ++g_checks;
            if (!sameFloat(got, rMul(a, b)))
                fail("mula.s", "random", "got " + hex(got));
        }
        // MOV/ABS/NEG, MFC1/MTC1, FCR31
        t.setF(0, 0xC0490FDBu);
        inst("fabs")(g_rdram, t.c, g_runtime);
        expectEq("abs.s", "x", t.getF(2), 0x40490FDBu);
        inst("fneg")(g_rdram, t.c, g_runtime); // f0 is still -pi
        expectEq("neg.s", "x", t.getF(2), 0x40490FDBu);
        t.gpr(4, 0xFFFFFFFF80000001ull);
        inst("mtc1_a0")(g_rdram, t.c, g_runtime);
        expectEq("mtc1", "bits", t.getF(0), 0x80000001u);
        inst("mfc1_v0")(g_rdram, t.c, g_runtime);
        expectEq("mfc1 sign-extends", "v0", t.lo(2), 0xFFFFFFFF80000001ull);
    }

    void testBranches()
    {
        Ctx t;
        struct B { const char *n; bool (*taken)(int64_t); bool likely; } bs[] = {
            {"bgez", [](int64_t v) { return v >= 0; }, false}, {"bltz", [](int64_t v) { return v < 0; }, false},
            {"blez", [](int64_t v) { return v <= 0; }, false}, {"bgtz", [](int64_t v) { return v > 0; }, false},
            {"bgezl", [](int64_t v) { return v >= 0; }, true}, {"bltzl", [](int64_t v) { return v < 0; }, true},
            {"blezl", [](int64_t v) { return v <= 0; }, true}, {"bgtzl", [](int64_t v) { return v > 0; }, true}};
        const int64_t values[] = {0, 1, -1, 0x80000000ll, 0x7FFFFFFFll, -0x80000000ll, 0x100000000ll, -0x100000000ll,
                                  0x00000000FFFFFFFFll, static_cast<int64_t>(0xFFFFFFFF00000000ull), INT64_MIN, INT64_MAX,
                                  static_cast<int64_t>(0xFFFFFFFF80000000ull) + 0x100000000ll};
        for (const auto &b : bs)
            for (int64_t v : values)
            {
                t.gpr(4, static_cast<uint64_t>(v));
                t.gpr(2, 0);
                t.gpr(31, 0xDEAD0000);
                t.c->pc = 0x1000;
                program(b.n)(g_rdram, t.c, g_runtime);
                // Taken: the delay slot (v0 = 1) runs and the branch skips "v0 += 10": v0 = 1.
                // Not taken: a branch-likely skips its delay slot (v0 = 10), others run it (v0 = 11).
                const uint64_t want = b.taken(v) ? 1 : (b.likely ? 10 : 11);
                expectEq(b.n, "v0", t.lo(2), want, "a0=" + hex(static_cast<uint64_t>(v)));
            }
        struct B2 { const char *n; bool eq; bool likely; } b2s[] = {{"beq", true, false}, {"bne", false, false}, {"beql", true, true}, {"bnel", false, true}};
        for (const auto &b : b2s)
            for (int same = 0; same < 2; ++same)
            {
                t.gpr(4, 0x1234567890ull);
                t.gpr(5, same ? 0x1234567890ull : 0x1234567891ull);
                t.gpr(2, 0);
                t.gpr(31, 0xDEAD0000);
                program(b.n)(g_rdram, t.c, g_runtime);
                const bool taken = (same != 0) == b.eq;
                expectEq(b.n, "v0", t.lo(2), static_cast<uint64_t>(taken ? 1 : (b.likely ? 10 : 11)));
            }
        // beq compares the whole 64-bit registers
        t.gpr(4, 0x100000001ull);
        t.gpr(5, 0x000000001ull);
        t.gpr(2, 0);
        program("beq")(g_rdram, t.c, g_runtime);
        expectEq("beq 64-bit", "v0", t.lo(2), 11ull);
        // The decision is made before the delay slot runs.
        t.gpr(4, 5);
        t.gpr(2, 0);
        program("delay_changes_cond")(g_rdram, t.c, g_runtime);
        expectEq("delay slot after the test", "v0", t.lo(2), 0ull); // branch taken: skips "v0 = 7"
        t.gpr(8, 0x2000);
        t.gpr(31, 0xDEAD0000);
        t.c->pc = 0;
        program("jr_delay_overwrites")(g_rdram, t.c, g_runtime);
        expectEq("jr reads its register first", "pc", static_cast<uint64_t>(t.c->pc), 0x2000ull);
        expectEq("jr delay slot ran", "t0", t.lo(8), 0x44ull);
    }

    void testIntegerAndHiLo()
    {
        Ctx t;
        for (int i = 0; i < 5000; ++i)
        {
            const int32_t a = static_cast<int32_t>(next()), b = static_cast<int32_t>(next() | (i % 5 == 0 ? 0 : 1));
            const uint32_t ua = static_cast<uint32_t>(a), ub = static_cast<uint32_t>(b);
            t.gpr(4, static_cast<uint64_t>(static_cast<int64_t>(a)), 0xAAAAull);
            t.gpr(5, static_cast<uint64_t>(static_cast<int64_t>(b)));
            auto sx = [](int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); };
            const int64_t p = static_cast<int64_t>(a) * b;
            const uint64_t up = static_cast<uint64_t>(ua) * ub;
            inst("mult")(g_rdram, t.c, g_runtime);
            expectEq("mult lo", "r", t.c->lo, sx(static_cast<int32_t>(p)));
            expectEq("mult hi", "r", t.c->hi, sx(static_cast<int32_t>(p >> 32)));
            inst("mult_rd")(g_rdram, t.c, g_runtime);
            expectEq("mult rd", "v0", t.lo(2), sx(static_cast<int32_t>(p)));
            inst("multu")(g_rdram, t.c, g_runtime);
            expectEq("multu lo", "r", t.c->lo, sx(static_cast<int32_t>(up)));
            expectEq("multu hi", "r", t.c->hi, sx(static_cast<int32_t>(up >> 32)));
            inst("mult1")(g_rdram, t.c, g_runtime);
            expectEq("mult1 lo1", "r", t.c->lo1, sx(static_cast<int32_t>(p)));
            expectEq("mult1 hi1", "r", t.c->hi1, sx(static_cast<int32_t>(p >> 32)));
            inst("mult1_rd")(g_rdram, t.c, g_runtime);
            expectEq("mult1 rd", "v0", t.lo(2), sx(static_cast<int32_t>(p)));
            inst("multu1")(g_rdram, t.c, g_runtime);
            expectEq("multu1 lo1", "r", t.c->lo1, sx(static_cast<int32_t>(up)));
            expectEq("multu1 hi1", "r", t.c->hi1, sx(static_cast<int32_t>(up >> 32)));
            // DIV: a / b with the EE's edge cases (INT_MIN / -1, divide by zero)
            auto divCheck = [&](const char *n, bool second, int32_t x, int32_t y, bool isSigned)
            {
                t.gpr(4, sx(x));
                t.gpr(5, sx(y));
                inst(n)(g_rdram, t.c, g_runtime);
                uint64_t wlo, whi;
                if (isSigned)
                {
                    if (y == 0)
                    {
                        wlo = x < 0 ? 1ull : ~0ull;
                        whi = sx(x);
                    }
                    else if (x == INT32_MIN && y == -1)
                    {
                        wlo = sx(INT32_MIN);
                        whi = 0;
                    }
                    else
                    {
                        wlo = sx(x / y);
                        whi = sx(x % y);
                    }
                }
                else
                {
                    const uint32_t ux = static_cast<uint32_t>(x), uy = static_cast<uint32_t>(y);
                    if (uy == 0)
                    {
                        wlo = ~0ull;
                        whi = sx(x);
                    }
                    else
                    {
                        wlo = sx(static_cast<int32_t>(ux / uy));
                        whi = sx(static_cast<int32_t>(ux % uy));
                    }
                }
                expectEq(n, "lo", second ? t.c->lo1 : t.c->lo, wlo, "x=" + hex(static_cast<uint32_t>(x)) + " y=" + hex(static_cast<uint32_t>(y)));
                expectEq(n, "hi", second ? t.c->hi1 : t.c->hi, whi, "x=" + hex(static_cast<uint32_t>(x)) + " y=" + hex(static_cast<uint32_t>(y)));
            };
            divCheck("div", false, a, b, true);
            divCheck("divu", false, a, b, false);
            divCheck("div1", true, a, b, true);
            divCheck("divu1", true, a, b, false);
            if (i % 50 == 0)
            {
                divCheck("div", false, INT32_MIN, -1, true);
                divCheck("div1", true, INT32_MIN, -1, true);
                divCheck("div", false, a, 0, true);
                divCheck("divu1", true, a, 0, false);
            }
            // MADD / MADDU with rd: HI:LO += a * b, rd = low
            t.c->hi = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(next())));
            t.c->lo = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(next())));
            const uint64_t acc = (static_cast<uint64_t>(static_cast<uint32_t>(t.c->hi)) << 32) | static_cast<uint32_t>(t.c->lo);
            t.gpr(4, sx(a));
            t.gpr(5, sx(b));
            inst("madd")(g_rdram, t.c, g_runtime);
            const uint64_t r1 = acc + static_cast<uint64_t>(p);
            expectEq("madd lo", "r", t.c->lo, sx(static_cast<int32_t>(r1)));
            expectEq("madd hi", "r", t.c->hi, sx(static_cast<int32_t>(r1 >> 32)));
            expectEq("madd rd", "v0", t.lo(2), sx(static_cast<int32_t>(r1)));
            // slt / sltu use the whole register
            const int64_t wa = static_cast<int64_t>(next()), wb = static_cast<int64_t>(next());
            t.gpr(4, static_cast<uint64_t>(wa));
            t.gpr(5, static_cast<uint64_t>(wb));
            inst("slt")(g_rdram, t.c, g_runtime);
            expectEq("slt", "v0", t.lo(2), static_cast<uint64_t>(wa < wb));
            inst("sltu")(g_rdram, t.c, g_runtime);
            expectEq("sltu", "v0", t.lo(2), static_cast<uint64_t>(static_cast<uint64_t>(wa) < static_cast<uint64_t>(wb)));
            // shifts
            t.gpr(4, static_cast<uint64_t>(wa));
            inst("dsra32")(g_rdram, t.c, g_runtime);
            expectEq("dsra32", "v0", t.lo(2), static_cast<uint64_t>(wa >> 35));
            inst("dsll32")(g_rdram, t.c, g_runtime);
            expectEq("dsll32", "v0", t.lo(2), static_cast<uint64_t>(wa) << 35);
            inst("dsrl32")(g_rdram, t.c, g_runtime);
            expectEq("dsrl32", "v0", t.lo(2), static_cast<uint64_t>(wa) >> 35);
            inst("sra")(g_rdram, t.c, g_runtime);
            expectEq("sra", "v0", t.lo(2), sx(static_cast<int32_t>(static_cast<uint32_t>(wa)) >> 4));
            inst("srl")(g_rdram, t.c, g_runtime);
            expectEq("srl", "v0", t.lo(2), sx(static_cast<int32_t>(static_cast<uint32_t>(wa) >> 4)));
            t.gpr(5, static_cast<uint64_t>(wb));
            inst("sllv")(g_rdram, t.c, g_runtime);
            expectEq("sllv", "v0", t.lo(2), sx(static_cast<int32_t>(static_cast<uint32_t>(wa) << (static_cast<uint32_t>(wb) & 31))));
        }
        // MFHI1 / MFLO1 / MTHI1 / MTLO1
        t.c->hi1 = 0x1122334455667788ull;
        t.c->lo1 = 0x99AABBCCDDEEFF00ull;
        inst("mfhi1")(g_rdram, t.c, g_runtime);
        expectEq("mfhi1", "v0", t.lo(2), 0x1122334455667788ull);
        inst("mflo1")(g_rdram, t.c, g_runtime);
        expectEq("mflo1", "v0", t.lo(2), 0x99AABBCCDDEEFF00ull);
        t.gpr(4, 0xCAFEBABE12345678ull);
        inst("mthi1")(g_rdram, t.c, g_runtime);
        inst("mtlo1")(g_rdram, t.c, g_runtime);
        expectEq("mthi1", "hi1", t.c->hi1, 0xCAFEBABE12345678ull);
        expectEq("mtlo1", "lo1", t.c->lo1, 0xCAFEBABE12345678ull);
        // MOVN / MOVZ move rs when rt is (non)zero
        t.gpr(4, 0x1111), t.gpr(5, 0), t.gpr(2, 0x77);
        inst("movn")(g_rdram, t.c, g_runtime);
        expectEq("movn rt=0", "v0", t.lo(2), 0x77ull);
        inst("movz")(g_rdram, t.c, g_runtime);
        expectEq("movz rt=0", "v0", t.lo(2), 0x1111ull);
    }

    // The unaligned loads and stores as byte moves, from the definition: little endian, the
    // address' low bits pick which bytes of the aligned word move.
    void testUnaligned()
    {
        Ctx t;
        const uint32_t base = 0x00100000;
        for (int i = 0; i < 4000; ++i)
        {
            uint8_t mem[16];
            for (auto &b : mem)
                b = static_cast<uint8_t>(next());
            const uint32_t off = static_cast<uint32_t>(next() % 16);
            const uint32_t a = base + off;
            const uint64_t reg = next();
            auto load = [&]() { std::memcpy(g_rdram + base, mem, 16); };
            // LWL / LWR (32-bit): the result is sign-extended; LWR at offset 0 replaces the word,
            // otherwise the upper half of the register stays.
            {
                const uint32_t o = off & 3, w = base + (off & ~3u);
                uint8_t m[4];
                std::memcpy(m, mem + (off & ~3u), 4);
                uint8_t r[4];
                uint32_t rl = static_cast<uint32_t>(reg);
                std::memcpy(r, &rl, 4);
                uint8_t nl[4], nr[4];
                std::memcpy(nl, r, 4);
                std::memcpy(nr, r, 4);
                for (uint32_t j = 0; j <= o; ++j)
                    nl[3 - j] = m[o - j];
                for (uint32_t j = 0; j <= 3 - o; ++j)
                    nr[j] = m[o + j];
                uint32_t wl, wr;
                std::memcpy(&wl, nl, 4);
                std::memcpy(&wr, nr, 4);
                (void)w;
                load();
                t.gpr(4, a);
                t.gpr(2, reg);
                inst("lwl")(g_rdram, t.c, g_runtime);
                expectEq("lwl", "v0", t.lo(2), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(wl))), "off=" + hex(off));
                t.gpr(2, reg);
                inst("lwr")(g_rdram, t.c, g_runtime);
                const uint64_t wantR = (o == 0) ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(wr)))
                                                : ((reg & 0xFFFFFFFF00000000ull) | wr);
                expectEq("lwr", "v0", t.lo(2), wantR, "off=" + hex(off) + " reg=" + hex(reg));
            }
            // LDL / LDR (64-bit)
            {
                const uint32_t o = off & 7;
                uint8_t m[8];
                std::memcpy(m, mem + (off & ~7u), 8);
                uint8_t nl[8], nr[8];
                std::memcpy(nl, &reg, 8);
                std::memcpy(nr, &reg, 8);
                for (uint32_t j = 0; j <= o; ++j)
                    nl[7 - j] = m[o - j];
                for (uint32_t j = 0; j <= 7 - o; ++j)
                    nr[j] = m[o + j];
                uint64_t wl, wr;
                std::memcpy(&wl, nl, 8);
                std::memcpy(&wr, nr, 8);
                load();
                t.gpr(4, a);
                t.gpr(2, reg);
                inst("ldl")(g_rdram, t.c, g_runtime);
                expectEq("ldl", "v0", t.lo(2), wl, "off=" + hex(off));
                t.gpr(2, reg);
                inst("ldr")(g_rdram, t.c, g_runtime);
                expectEq("ldr", "v0", t.lo(2), wr, "off=" + hex(off));
            }
            // SWL / SWR / SDL / SDR
            {
                const uint32_t o = off & 3;
                uint8_t want[16];
                std::memcpy(want, mem, 16);
                uint8_t r[8];
                std::memcpy(r, &reg, 8);
                uint8_t *w = want + (off & ~3u);
                for (uint32_t j = 0; j <= o; ++j)
                    w[o - j] = r[3 - j];
                load();
                t.gpr(4, a);
                t.gpr(5, reg);
                inst("swl")(g_rdram, t.c, g_runtime);
                ++g_checks;
                if (std::memcmp(g_rdram + base, want, 16) != 0)
                    fail("swl", "bytes", "off=" + hex(off));
                std::memcpy(want, mem, 16);
                w = want + (off & ~3u);
                for (uint32_t j = 0; j <= 3 - o; ++j)
                    w[o + j] = r[j];
                load();
                inst("swr")(g_rdram, t.c, g_runtime);
                ++g_checks;
                if (std::memcmp(g_rdram + base, want, 16) != 0)
                    fail("swr", "bytes", "off=" + hex(off));
                const uint32_t o8 = off & 7;
                std::memcpy(want, mem, 16);
                w = want + (off & ~7u);
                for (uint32_t j = 0; j <= o8; ++j)
                    w[o8 - j] = r[7 - j];
                load();
                inst("sdl")(g_rdram, t.c, g_runtime);
                ++g_checks;
                if (std::memcmp(g_rdram + base, want, 16) != 0)
                    fail("sdl", "bytes", "off=" + hex(off));
                std::memcpy(want, mem, 16);
                w = want + (off & ~7u);
                for (uint32_t j = 0; j <= 7 - o8; ++j)
                    w[o8 + j] = r[j];
                load();
                inst("sdr")(g_rdram, t.c, g_runtime);
                ++g_checks;
                if (std::memcmp(g_rdram + base, want, 16) != 0)
                    fail("sdr", "bytes", "off=" + hex(off));
            }
            // Plain loads sign- or zero-extend; LQ / SQ ignore the low four address bits.
            load();
            const uint32_t ea = base + (off & ~7u);
            t.gpr(4, ea);
            inst("lb")(g_rdram, t.c, g_runtime);
            expectEq("lb", "v0", t.lo(2), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(mem[off & ~7u]))));
            inst("lbu")(g_rdram, t.c, g_runtime);
            expectEq("lbu", "v0", t.lo(2), static_cast<uint64_t>(mem[off & ~7u]));
            uint16_t h;
            std::memcpy(&h, mem + (off & ~7u), 2);
            inst("lh")(g_rdram, t.c, g_runtime);
            expectEq("lh", "v0", t.lo(2), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(h))));
            inst("lhu")(g_rdram, t.c, g_runtime);
            expectEq("lhu", "v0", t.lo(2), static_cast<uint64_t>(h));
            uint32_t w32;
            std::memcpy(&w32, mem + (off & ~7u), 4);
            inst("lw")(g_rdram, t.c, g_runtime);
            expectEq("lw", "v0", t.lo(2), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w32))));
            inst("lwu")(g_rdram, t.c, g_runtime);
            expectEq("lwu", "v0", t.lo(2), static_cast<uint64_t>(w32));
            t.gpr(4, base + off); // any alignment: the quadword ops mask it
            inst("lq")(g_rdram, t.c, g_runtime);
            ++g_checks;
            if (std::memcmp(&t.c->r[2], mem, 16) != 0)
                fail("lq", "aligned down", "off=" + hex(off));
            const uint64_t lo = next(), hi = next();
            t.gpr(5, lo, hi);
            std::memcpy(g_rdram + base, mem, 16);
            inst("sq")(g_rdram, t.c, g_runtime);
            uint64_t got[2];
            std::memcpy(got, g_rdram + base, 16);
            ++g_checks;
            if (got[0] != lo || got[1] != hi)
                fail("sq", "aligned down", "off=" + hex(off));
        }
    }

    // ---------------------------------------------------------------- MMI
    struct V128
    {
        uint32_t w[4];
    };
    V128 rnd128()
    {
        V128 v;
        for (auto &x : v.w)
            x = static_cast<uint32_t>(next());
        if (next() % 4 == 0)
            v.w[next() % 4] = (next() & 1) ? 0x80000000u : 0x7FFFFFFFu;
        return v;
    }
    void put(Ctx &t, int r, const V128 &v) { std::memcpy(&t.c->r[r], v.w, 16); }
    V128 get(Ctx &t, int r)
    {
        V128 v;
        std::memcpy(v.w, &t.c->r[r], 16);
        return v;
    }
    bool eq(const V128 &a, const V128 &b) { return std::memcmp(a.w, b.w, 16) == 0; }
    std::string show(const V128 &v)
    {
        char b[80];
        std::snprintf(b, sizeof b, "%08x_%08x_%08x_%08x", v.w[3], v.w[2], v.w[1], v.w[0]);
        return b;
    }

    template <typename F>
    void mmi2(const char *name, F f)
    {
        Ctx t;
        for (int i = 0; i < 2000; ++i)
        {
            const V128 a = rnd128(), b = rnd128();
            put(t, 4, a);
            put(t, 5, b);
            put(t, 2, rnd128());
            inst(name)(g_rdram, t.c, g_runtime);
            const V128 want = f(a, b); // a = rs, b = rt
            ++g_checks;
            if (!eq(get(t, 2), want))
                fail(name, "random", "rs=" + show(a) + " rt=" + show(b) + " got " + show(get(t, 2)) + " want " + show(want));
        }
    }
    int16_t h(const V128 &v, int i) { return static_cast<int16_t>((v.w[i / 2] >> ((i & 1) * 16)) & 0xFFFF); }
    uint8_t by(const V128 &v, int i) { return static_cast<uint8_t>((v.w[i / 4] >> ((i & 3) * 8)) & 0xFF); }
    void sh(V128 &v, int i, uint16_t x) { v.w[i / 2] = (v.w[i / 2] & ~(0xFFFFu << ((i & 1) * 16))) | (static_cast<uint32_t>(x) << ((i & 1) * 16)); }
    void sb(V128 &v, int i, uint8_t x) { v.w[i / 4] = (v.w[i / 4] & ~(0xFFu << ((i & 3) * 8))) | (static_cast<uint32_t>(x) << ((i & 3) * 8)); }

    void testMmi()
    {
        mmi2("paddw", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] + b.w[i]; return r; });
        mmi2("psubw", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] - b.w[i]; return r; });
        mmi2("pmaxw", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = static_cast<uint32_t>(std::max(static_cast<int32_t>(a.w[i]), static_cast<int32_t>(b.w[i]))); return r; });
        mmi2("paddh", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 8; ++i) sh(r, i, static_cast<uint16_t>(h(a, i) + h(b, i))); return r; });
        mmi2("psubh", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 8; ++i) sh(r, i, static_cast<uint16_t>(h(a, i) - h(b, i))); return r; });
        mmi2("paddb", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 16; ++i) sb(r, i, static_cast<uint8_t>(by(a, i) + by(b, i))); return r; });
        mmi2("psubb", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 16; ++i) sb(r, i, static_cast<uint8_t>(by(a, i) - by(b, i))); return r; });
        mmi2("paddsw", [](V128 a, V128 b) {
            V128 r;
            for (int i = 0; i < 4; ++i)
            {
                const int64_t s = static_cast<int64_t>(static_cast<int32_t>(a.w[i])) + static_cast<int32_t>(b.w[i]);
                r.w[i] = static_cast<uint32_t>(s > INT32_MAX ? INT32_MAX : (s < INT32_MIN ? INT32_MIN : s));
            }
            return r; });
        mmi2("psubsw", [](V128 a, V128 b) {
            V128 r;
            for (int i = 0; i < 4; ++i)
            {
                const int64_t s = static_cast<int64_t>(static_cast<int32_t>(a.w[i])) - static_cast<int32_t>(b.w[i]);
                r.w[i] = static_cast<uint32_t>(s > INT32_MAX ? INT32_MAX : (s < INT32_MIN ? INT32_MIN : s));
            }
            return r; });
        // PEXTLW rd, rs, rt: rd = rs[1] rt[1] rs[0] rt[0] (rt in the even words)
        mmi2("pextlw", [](V128 a, V128 b) { return V128{{b.w[0], a.w[0], b.w[1], a.w[1]}}; });
        mmi2("pextuw", [](V128 a, V128 b) { return V128{{b.w[2], a.w[2], b.w[3], a.w[3]}}; });
        mmi2("pextlh", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 4; ++i) { sh(r, 2 * i, static_cast<uint16_t>(h(b, i))); sh(r, 2 * i + 1, static_cast<uint16_t>(h(a, i))); } return r; });
        mmi2("pextlb", [](V128 a, V128 b) { V128 r{}; for (int i = 0; i < 8; ++i) { sb(r, 2 * i, by(b, i)); sb(r, 2 * i + 1, by(a, i)); } return r; });
        // PPACW rd, rs, rt: rd = rs[2] rs[0] rt[2] rt[0]
        mmi2("ppacw", [](V128 a, V128 b) { return V128{{b.w[0], b.w[2], a.w[0], a.w[2]}}; });
        // PCPYLD rd, rs, rt: rd = (rs.lo, rt.lo) as (high, low); PCPYUD: (rs.hi, rt.hi) -> rd.low = rt.hi
        mmi2("pcpyld", [](V128 a, V128 b) { return V128{{b.w[0], b.w[1], a.w[0], a.w[1]}}; });
        mmi2("pcpyud", [](V128 a, V128 b) { return V128{{a.w[2], a.w[3], b.w[2], b.w[3]}}; });
        mmi2("pand", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] & b.w[i]; return r; });
        mmi2("pxor", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] ^ b.w[i]; return r; });
        mmi2("por", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] | b.w[i]; return r; });
        mmi2("pnor", [](V128 a, V128 b) { V128 r; for (int i = 0; i < 4; ++i) r.w[i] = ~(a.w[i] | b.w[i]); return r; });
        // one-source ops: rt is the source
        mmi2("pcpyh", [](V128, V128 b) { V128 r{}; for (int i = 0; i < 4; ++i) sh(r, i, static_cast<uint16_t>(h(b, 0))); for (int i = 4; i < 8; ++i) sh(r, i, static_cast<uint16_t>(h(b, 4))); return r; });
        mmi2("pexeh", [](V128, V128 b) { V128 r = b; sh(r, 0, static_cast<uint16_t>(h(b, 2))); sh(r, 2, static_cast<uint16_t>(h(b, 0))); sh(r, 4, static_cast<uint16_t>(h(b, 6))); sh(r, 6, static_cast<uint16_t>(h(b, 4))); return r; });
        mmi2("pexch", [](V128, V128 b) { V128 r = b; sh(r, 1, static_cast<uint16_t>(h(b, 2))); sh(r, 2, static_cast<uint16_t>(h(b, 1))); sh(r, 5, static_cast<uint16_t>(h(b, 6))); sh(r, 6, static_cast<uint16_t>(h(b, 5))); return r; });
        mmi2("pexcw", [](V128, V128 b) { return V128{{b.w[0], b.w[2], b.w[1], b.w[3]}}; });
        mmi2("pexew", [](V128, V128 b) { return V128{{b.w[2], b.w[1], b.w[0], b.w[3]}}; });
        mmi2("prevh", [](V128, V128 b) { V128 r{}; for (int i = 0; i < 4; ++i) { sh(r, i, static_cast<uint16_t>(h(b, 3 - i))); sh(r, 4 + i, static_cast<uint16_t>(h(b, 7 - i))); } return r; });
        mmi2("prot3w", [](V128, V128 b) { return V128{{b.w[1], b.w[2], b.w[0], b.w[3]}}; });
    }

    // ---------------------------------------------------------------- VU0 macro mode
    // The VU's FMAC: operands flushed, results truncated, overflow to MAX; the same soft float.
    void putVf(Ctx &t, int r, const uint32_t v[4]) { std::memcpy(&t.c->vu0_vf[r], v, 16); }
    void getVf(Ctx &t, int r, uint32_t v[4]) { std::memcpy(v, &t.c->vu0_vf[r], 16); }
    void getAcc(Ctx &t, uint32_t v[4]) { std::memcpy(v, &t.c->vu0_acc, 16); }
    void putAcc(Ctx &t, const uint32_t v[4]) { std::memcpy(&t.c->vu0_acc, v, 16); }

    bool laneInDest(int lane, int dest) { return (dest >> (3 - lane)) & 1; } // x = bit 3

    void checkVec(const char *name, int dest, const uint32_t got[4], const uint32_t want[4], const uint32_t old[4], const std::string &detail)
    {
        for (int l = 0; l < 4; ++l)
        {
            const uint32_t w = laneInDest(l, dest) ? want[l] : old[l];
            ++g_checks;
            if (!(laneInDest(l, dest) ? sameFloat(got[l], w) : got[l] == w))
                fail(name, "lane", "lane " + std::to_string(l) + " dest=" + hex(dest) + " got " + hex(got[l]) + " want " + hex(w) + " " + detail);
        }
    }

    void testVu0()
    {
        Ctx t;
        for (int iter = 0; iter < 4000; ++iter)
        {
            uint32_t fs[4], ft[4], fd[4], acc[4];
            for (int l = 0; l < 4; ++l)
            {
                fs[l] = operand();
                ft[l] = operand();
                fd[l] = operand();
                acc[l] = operand();
            }
            auto run = [&](const char *base, int dest, auto model, bool intoAcc, int dreg = 3)
            {
                char n[48];
                std::snprintf(n, sizeof n, "%s_%X", base, dest);
                putVf(t, 1, fs);
                putVf(t, 2, ft);
                putVf(t, 3, fd);
                putAcc(t, acc);
                inst(n)(g_rdram, t.c, g_runtime);
                uint32_t want[4];
                for (int l = 0; l < 4; ++l)
                    want[l] = model(l);
                uint32_t got[4];
                if (intoAcc)
                    getAcc(t, got);
                else
                    getVf(t, dreg, got);
                const std::string d = "fs=" + hex(fs[0]) + "," + hex(fs[1]) + " ft=" + hex(ft[0]) + "," + hex(ft[1]);
                checkVec(n, dest, got, want, intoAcc ? acc : (dreg == 3 ? fd : ft), d);
            };
            for (int dest : {0xF, 0x8, 0x5, 0x3})
            {
                run("vadd", dest, [&](int l) { return rAdd(fs[l], ft[l]); }, false);
                run("vsub", dest, [&](int l) { return rAdd(fs[l], ft[l], true); }, false);
                run("vmul", dest, [&](int l) { return rMul(fs[l], ft[l]); }, false);
                run("vmadd", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[l])); }, false);
                run("vmsub", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[l]), true); }, false);
                run("vmaddx", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[0])); }, false);
                run("vmaddw", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[3])); }, false);
                run("vmulx", dest, [&](int l) { return rMul(fs[l], ft[0]); }, false);
                run("vmulw", dest, [&](int l) { return rMul(fs[l], ft[3]); }, false);
                run("vaddy", dest, [&](int l) { return rAdd(fs[l], ft[1]); }, false);
                run("vsubz", dest, [&](int l) { return rAdd(fs[l], ft[2], true); }, false);
                run("vmula", dest, [&](int l) { return rMul(fs[l], ft[l]); }, true);
                run("vmulax", dest, [&](int l) { return rMul(fs[l], ft[0]); }, true);
                run("vmadda", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[l])); }, true);
                run("vmaddax", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[0])); }, true);
                run("vmadday", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[1])); }, true);
                run("vmaddaz", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[2])); }, true);
                run("vmaddaw", dest, [&](int l) { return rAdd(acc[l], rMul(fs[l], ft[3])); }, true);
                run("vadda", dest, [&](int l) { return rAdd(fs[l], ft[l]); }, true);
                run("vsuba", dest, [&](int l) { return rAdd(fs[l], ft[l], true); }, true);
                run("vmove", dest, [&](int l) { return fs[l]; }, false, 2);
                run("vmr32", dest, [&](int l) { return fs[(l + 1) & 3]; }, false, 2);
            }
        }
        // The MAC flags: an overflow clamps to MAX and sets O (bits 12-15 of MAC, 0x8 and 0x200 of
        // the status); a negative result sets S, a zero result Z.
        {
            const uint32_t big[4] = {0x7F7FFFFFu, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x40000000u};
            const uint32_t two[4] = {0x40000000u, 0x40000000u, 0x40000000u, 0x00000000u};
            putVf(t, 1, big);
            putVf(t, 2, two);
            t.c->vu0_mac_flags = 0;
            t.c->vu0_status = 0;
            inst("vmul_F")(g_rdram, t.c, g_runtime);
            expectEq("vmul MAC overflow", "O bits", t.c->vu0_mac_flags & 0xF000u, 0xE000u); // x, y, z overflow
            expectEq("vmul MAC sign", "S bits", t.c->vu0_mac_flags & 0x00F0u, 0x0020u);     // z is negative
            expectEq("vmul MAC zero", "Z bits", t.c->vu0_mac_flags & 0x000Fu, 0x0001u);     // w = 2 * 0 = 0
            expectEq("vmul status O", "bit", static_cast<uint32_t>(t.c->vu0_status & 0x208u), 0x208u);
        }
        // OPMSUB: fd.xyz = ACC.xyz - fs.yzx * ft.zxy
        for (int iter = 0; iter < 2000; ++iter)
        {
            uint32_t fs[4], ft[4], fd[4], acc[4];
            for (int l = 0; l < 4; ++l)
            {
                fs[l] = operand();
                ft[l] = operand();
                fd[l] = operand();
                acc[l] = operand();
            }
            putVf(t, 1, fs);
            putVf(t, 2, ft);
            putVf(t, 3, fd);
            putAcc(t, acc);
            inst("vopmsub")(g_rdram, t.c, g_runtime);
            uint32_t got[4];
            getVf(t, 3, got);
            const uint32_t want[4] = {rAdd(acc[0], rMul(fs[1], ft[2]), true), rAdd(acc[1], rMul(fs[2], ft[0]), true),
                                      rAdd(acc[2], rMul(fs[0], ft[1]), true), fd[3]};
            for (int l = 0; l < 4; ++l)
            {
                ++g_checks;
                if (!sameFloat(got[l], want[l]))
                    fail("vopmsub", "lane", "lane " + std::to_string(l) + " got " + hex(got[l]) + " want " + hex(want[l]));
            }
            inst("vopmula")(g_rdram, t.c, g_runtime);
            getAcc(t, got);
            const uint32_t wantA[4] = {rMul(fs[1], ft[2]), rMul(fs[2], ft[0]), rMul(fs[0], ft[1]), acc[3]};
            for (int l = 0; l < 3; ++l)
            {
                ++g_checks;
                if (!sameFloat(got[l], wantA[l]))
                    fail("vopmula", "lane", "lane " + std::to_string(l) + " got " + hex(got[l]) + " want " + hex(wantA[l]));
            }
        }
        // Q: VDIV / VSQRT / VRSQRT with field selects
        for (int iter = 0; iter < 1500; ++iter)
        {
            uint32_t fs[4], ft[4];
            for (int l = 0; l < 4; ++l)
            {
                fs[l] = operand();
                ft[l] = operand();
            }
            for (int fsf = 0; fsf < 4; ++fsf)
                for (int ftf = 0; ftf < 4; ++ftf)
                {
                    char n[40];
                    putVf(t, 1, fs);
                    putVf(t, 2, ft);
                    uint32_t q;
                    std::snprintf(n, sizeof n, "vdiv_%d_%d", fsf, ftf);
                    inst(n)(g_rdram, t.c, g_runtime);
                    std::memcpy(&q, &t.c->vu0_q, 4);
                    ++g_checks;
                    if (!sameFloat(q, rDiv(fs[fsf], ft[ftf])))
                        fail("vdiv", n, "got " + hex(q) + " want " + hex(rDiv(fs[fsf], ft[ftf])));
                    std::snprintf(n, sizeof n, "vsqrt_%d_%d", fsf, ftf);
                    inst(n)(g_rdram, t.c, g_runtime);
                    std::memcpy(&q, &t.c->vu0_q, 4);
                    ++g_checks;
                    if (!sameFloat(q, rSqrtAbs(ft[ftf])))
                        fail("vsqrt", n, "got " + hex(q) + " want " + hex(rSqrtAbs(ft[ftf])));
                    std::snprintf(n, sizeof n, "vrsqrt_%d_%d", fsf, ftf);
                    inst(n)(g_rdram, t.c, g_runtime);
                    std::memcpy(&q, &t.c->vu0_q, 4);
                    if (!isZeroBits(ft[ftf]))
                    {
                        const uint32_t want = rDiv(fs[fsf], rSqrtAbs(ft[ftf]));
                        ++g_checks;
                        if (!sameFloat(q, want))
                            fail("vrsqrt", n, "got " + hex(q) + " want " + hex(want));
                    }
                }
        }
        // ITOF / FTOI
        for (int iter = 0; iter < 3000; ++iter)
        {
            uint32_t fs[4], fd[4];
            for (int l = 0; l < 4; ++l)
            {
                fs[l] = (iter % 2) ? static_cast<uint32_t>(next()) : operand();
                fd[l] = static_cast<uint32_t>(next());
            }
            struct M { const char *n; int frac; bool toInt; } ms[] = {{"vitof4", 4, false}, {"vitof12", 12, false}, {"vftoi4", 4, true}, {"vftoi0", 0, true}};
            for (const auto &m : ms)
                for (int dest : {0xF, 0x5})
                {
                    char n[40];
                    std::snprintf(n, sizeof n, "%s_%X", m.n, dest);
                    uint32_t src[4];
                    for (int l = 0; l < 4; ++l)
                        src[l] = m.toInt ? operand() : fs[l]; // floats for FTOI, any int for ITOF
                    putVf(t, 1, src);
                    putVf(t, 2, fd); // the destination of these is the ft field (vf2)
                    uint32_t want[4];
                    for (int l = 0; l < 4; ++l)
                        want[l] = m.toInt ? rFtoi(src[l], m.frac) : rItof(static_cast<int32_t>(src[l]), m.frac);
                    inst(n)(g_rdram, t.c, g_runtime);
                    uint32_t got[4];
                    getVf(t, 2, got);
                    if (m.toInt && iter % 2 == 0)
                    {
                        // ints compare exactly; skip inputs with exponent 255
                    }
                    checkVec(n, dest, got, want, fd, "fs=" + hex(fs[0]));
                }
        }
    }
}

int main()
{
    // As EeScheduler::run does for the executor thread. RT_EE_TEST_IEEE=1 leaves the thread in the
    // host's default mode to show what the float cases catch there (they fail).
    const char *ieee = std::getenv("RT_EE_TEST_IEEE");
    if (!(ieee && *ieee == '1'))
        ps2_ee_fpu::enterThread();
    g_rdram = static_cast<uint8_t *>(std::calloc(PS2_RAM_SIZE, 1));
    if (!g_rdram)
        return 2;
    testFloatMode();
    testFpu();
    testBranches();
    testIntegerAndHiLo();
    testUnaligned();
    testMmi();
    testVu0();
    if (g_unexpected)
    {
        std::printf("FAIL %d calls into runtime entry points a case should not reach\n", g_unexpected);
        ++g_failed;
    }
    std::printf("ee_semantics_test: %d checks, %d failed\n", g_checks, g_failed);
    std::free(g_rdram);
    return g_failed ? 1 : 0;
}
