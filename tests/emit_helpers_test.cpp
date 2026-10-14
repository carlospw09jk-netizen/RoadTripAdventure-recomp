// The recompiler's emission helpers that decide what generated code stores and how it reads:
// which translated instructions can observe ctx->pc (pc_use.h), the READn -> READnPC rewrite, and the
// readability pass (code_beautifier.h). Header-only; no game data.
#include "ps2recomp/Emitters/code_beautifier.h"
#include "ps2recomp/Emitters/pc_use.h"

#include <cstdio>
#include <string>

using ps2recomp::PcUse;

static int g_failures = 0;

static void expect(bool ok, const char *what, const std::string &detail = {})
{
    if (!ok)
    {
        ++g_failures;
        std::fprintf(stderr, "FAIL: %s %s\n", what, detail.c_str());
    }
}

static void pcUse(const char *code, PcUse want)
{
    const PcUse got = ps2recomp::classifyPcUse(code);
    expect(got == want, code, "got " + std::to_string(static_cast<int>(got)) + ", want " + std::to_string(static_cast<int>(want)));
}

int main()
{
    // pure arithmetic: no pc
    pcUse("SET_GPR_S32(ctx, 29, (int32_t)ADD32(GPR_U32(ctx, 29), 4294967184));", PcUse::None);
    pcUse("SET_GPR_U64(ctx, 18, (uint64_t)GPR_U64(ctx, 5) + (uint64_t)GPR_U64(ctx, 0));", PcUse::None);
    pcUse("ctx->f[1] = FPU_SUB_S(ctx->f[7], ctx->f[11]);", PcUse::None);
    pcUse("// NOP", PcUse::None);
    // plain memory: the access publishes the pc itself
    pcUse("SET_GPR_S32(ctx, 8, (int32_t)READ32(ADD32(GPR_U32(ctx, 29), 16)));", PcUse::MemoryOnly);
    pcUse("WRITE64(ADD32(GPR_U32(ctx, 29), 32), GPR_U64(ctx, 16));", PcUse::MemoryOnly);
    pcUse("{ uint32_t bits = READ32(ADD32(GPR_U32(ctx, 7), 8)); float f; std::memcpy(&f, &bits, sizeof(f)); ctx->f[11] = f; }",
          PcUse::MemoryOnly);
    // anything that reaches the runtime, raises, or is unknown: store the pc
    pcUse("runtime->Load32(rdram, ctx, 0x1000F430u);", PcUse::Full);
    pcUse("{ uint32_t tmp; bool ov; ADD32_OV(GPR_U32(ctx, 4), (int32_t)1, tmp, ov); if (ov) runtime->SignalException(ctx, EXCEPTION_INTEGER_OVERFLOW); }",
          PcUse::Full);
    pcUse("ps2_syscalls::Exit(rdram, ctx, runtime);", PcUse::Full);
    pcUse("SOMETHING_NEW(ctx, 1);", PcUse::Full);
    pcUse("SET_GPR_S32(ctx, 8, (int32_t)READ32(addr)); handleSyscall(rdram, ctx);", PcUse::Full);

    // the rewrite
    expect(ps2recomp::withPcMemoryMacros("SET_GPR_S32(ctx, 8, (int32_t)READ32(ADD32(GPR_U32(ctx, 29), 16)));", 0x2217F8u) ==
               "SET_GPR_S32(ctx, 8, (int32_t)READ32PC(0x2217F8u, ADD32(GPR_U32(ctx, 29), 16)));",
           "READ32 -> READ32PC");
    expect(ps2recomp::withPcMemoryMacros("WRITE128(a, b); FAST_WRITE128(c, d);", 0x10u) ==
               "WRITE128PC(0x10u, a, b); FAST_WRITE128(c, d);",
           "WRITE128 rewritten, FAST_WRITE128 left");

    // the readability pass
    using ps2recomp::beautifyGeneratedCode;
    expect(beautifyGeneratedCode("SET_GPR_S32(ctx, 29, (int32_t)ADD32(GPR_U32(ctx, 29), 4294967184));") ==
               "SET_GPR_S32(ctx, R_SP, (int32_t)ADD32(GPR_U32(ctx, R_SP), -112));",
           "names and negative immediate");
    expect(beautifyGeneratedCode("SET_GPR_U64(ctx, 18, (uint64_t)GPR_U64(ctx, 5) + (uint64_t)GPR_U64(ctx, 0));") ==
               "SET_GPR_U64(ctx, R_S2, (uint64_t)GPR_U64(ctx, R_A1));",
           "move");
    expect(beautifyGeneratedCode("SET_GPR_S32(ctx, 2, (int32_t)ADD32(GPR_U32(ctx, 0), 32));") ==
               "SET_GPR_S32(ctx, R_V0, (int32_t)32u);",
           "li");
    expect(beautifyGeneratedCode("x = READ32(ADD32(GPR_U32(ctx, 8), 0));") == "x = READ32(GPR_U32(ctx, R_T0));", "+ 0");
    expect(beautifyGeneratedCode("SET_GPR_U32(ctx, 31, 0x2218B0u); getRegU32(ctx, 31);") ==
               "SET_GPR_U32(ctx, R_RA, 0x2218B0u); getRegU32(ctx, R_RA);",
           "link register");
    // not a register accessor: left alone
    expect(beautifyGeneratedCode("foo(ctx, 12)") == "foo(ctx, 12)", "other functions untouched");

    if (g_failures == 0)
        std::puts("emit_helpers_test: ok");
    return g_failures == 0 ? 0 : 1;
}
