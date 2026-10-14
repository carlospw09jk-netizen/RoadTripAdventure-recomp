// Generates the code under test for ee_semantics_test: single R5900 instructions and small
// programs, translated by the real recompiler (decoder + translators + function emitter), as C++
// that the test compiles against the runtime headers. The test then runs them against reference
// models written from the R5900 / VU manuals (it does not read this file's encodings back).
//
//   ee_semantics_gen <out.inc>
//
// Instruction case:  static void c_<name>(rdram, ctx, runtime) { <translation> }
// Program case:      the whole recompiled function, entry `p_<name>`; its first word is at 0x1000.

#include "ps2recomp/code_generator.h"
#include "ps2recomp/r5900_decoder.h"
#include "ps2recomp/types.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ps2recomp;

namespace
{
    constexpr uint32_t R(uint32_t op, uint32_t rs, uint32_t rt, uint32_t rd, uint32_t sa, uint32_t fn)
    {
        return (op << 26) | (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | fn;
    }
    constexpr uint32_t I(uint32_t op, uint32_t rs, uint32_t rt, uint32_t imm)
    {
        return (op << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF);
    }
    constexpr uint32_t SPECIAL(uint32_t rs, uint32_t rt, uint32_t rd, uint32_t sa, uint32_t fn) { return R(0, rs, rt, rd, sa, fn); }
    constexpr uint32_t COP1S(uint32_t ft, uint32_t fs, uint32_t fd, uint32_t fn) { return R(0x11, 0x10, ft, fs, fd, fn); }
    constexpr uint32_t COP1W(uint32_t ft, uint32_t fs, uint32_t fd, uint32_t fn) { return R(0x11, 0x14, ft, fs, fd, fn); }
    constexpr uint32_t MMI(uint32_t rs, uint32_t rt, uint32_t rd, uint32_t sa, uint32_t fn) { return R(0x1C, rs, rt, rd, sa, fn); }
    // VU0 macro instruction, special1 (function < 0x3C): dest, ft, fs, fd, function.
    constexpr uint32_t VU1(uint32_t dest, uint32_t ft, uint32_t fs, uint32_t fd, uint32_t fn)
    {
        return (0x12u << 26) | (1u << 25) | (dest << 21) | (ft << 16) | (fs << 11) | (fd << 6) | fn;
    }
    // special2: op7 = (bits 10..6 << 2) | bits 1..0, function bits 5..2 = 0xF.
    constexpr uint32_t VU2(uint32_t dest, uint32_t ft, uint32_t fs, uint32_t op7)
    {
        return (0x12u << 26) | (1u << 25) | (dest << 21) | (ft << 16) | (fs << 11) | (((op7 >> 2) & 0x1F) << 6) | 0x3C | (op7 & 3);
    }

    struct Gen
    {
        std::vector<Symbol> symbols;
        std::vector<Section> sections;
        R5900Decoder decoder;
        CodeGenerator cg{symbols, sections};
        std::ostringstream out;
        std::vector<std::string> names;
        std::vector<std::string> programs;

        Gen() { cg.setEmitInstructionComments(false); }

        void inst(const std::string &name, uint32_t word, uint32_t pc = 0x1000)
        {
            Instruction i = decoder.decodeInstruction(pc, word, false);
            out << "static void c_" << name << "(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)\n{\n"
                << "    (void)rdram; (void)runtime; (void)ctx;\n    " << cg.translateInstruction(i) << "\n}\n";
            names.push_back(name);
        }

        void program(const std::string &name, const std::vector<uint32_t> &words)
        {
            Function f;
            f.name = "p_" + name;
            f.start = 0x1000;
            f.end = 0x1000 + 4 * static_cast<uint32_t>(words.size());
            std::vector<Instruction> insts;
            for (size_t k = 0; k < words.size(); ++k)
                insts.push_back(decoder.decodeInstruction(0x1000 + 4 * static_cast<uint32_t>(k), words[k], false));
            f.instructions = insts;
            std::string code = cg.generateFunction(f, insts, false);
            // The emitted function is named by the generator; give it the program's name.
            size_t paren = code.find("(uint8_t");
            if (paren == std::string::npos)
            {
                std::fprintf(stderr, "no function in the output for %s\n", name.c_str());
                std::exit(1);
            }
            size_t b = paren;
            while (b > 0 && (std::isalnum(static_cast<unsigned char>(code[b - 1])) || code[b - 1] == '_'))
                --b;
            const std::string fname = code.substr(b, paren - b);
            if (fname.empty())
            {
                std::fprintf(stderr, "no function name for %s\n", name.c_str());
                std::exit(1);
            }
            size_t pos = code.find(fname);
            while (pos != std::string::npos)
            {
                code.replace(pos, fname.size(), "p_" + name);
                pos = code.find(fname, pos + 2 + name.size());
            }
            out << "static " << code << "\n";
            programs.push_back(name);
        }
    };
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: ee_semantics_gen <out.inc>\n");
        return 1;
    }
    Gen g;
    // Registers used by the cases: a0=$4, a1=$5, v0=$2, v1=$3, t0=$8, t1=$9.
    // ---- 64-bit sign tests: the branch reads the whole register
    // 0x1000: <branch> a0, +2 ; 0x1004: addiu v0,$0,1 (delay) ; 0x1008: addiu v0,v0,10 ; 0x100c: jr ra ; nop
    auto branchProgram = [&](const std::string &name, uint32_t branchWord)
    {
        g.program(name, {branchWord, I(0x09, 0, 2, 1), I(0x09, 2, 2, 10), SPECIAL(31, 0, 0, 0, 0x08), 0});
    };
    branchProgram("bgez", I(0x01, 4, 1, 2));
    branchProgram("bltz", I(0x01, 4, 0, 2));
    branchProgram("blez", I(0x06, 4, 0, 2));
    branchProgram("bgtz", I(0x07, 4, 0, 2));
    branchProgram("bgezl", I(0x01, 4, 3, 2));
    branchProgram("bltzl", I(0x01, 4, 2, 2));
    branchProgram("blezl", I(0x16, 4, 0, 2));
    branchProgram("bgtzl", I(0x17, 4, 0, 2));
    // beq/bne/beql/bnel a0, a1
    auto branch2 = [&](const std::string &name, uint32_t op)
    { g.program(name, {I(op, 4, 5, 2), I(0x09, 0, 2, 1), I(0x09, 2, 2, 10), SPECIAL(31, 0, 0, 0, 0x08), 0}); };
    branch2("beq", 0x04);
    branch2("bne", 0x05);
    branch2("beql", 0x14);
    branch2("bnel", 0x15);
    // A delay slot that changes the register the branch tested must not change the decision:
    // bne a0,$0,+2 ; addiu a0,$0,0 (delay) ; addiu v0,$0,7 ; jr ra ; nop  -> taken target = 0x100c
    g.program("delay_changes_cond", {I(0x05, 4, 0, 2), I(0x09, 0, 4, 0), I(0x09, 0, 2, 7), SPECIAL(31, 0, 0, 0, 0x08), 0});
    // jr with a delay slot that overwrites the register: jr t0 ; addiu t0,$0,0x44 -> goes to the old t0
    g.program("jr_delay_overwrites", {SPECIAL(8, 0, 0, 0, 0x08), I(0x09, 0, 8, 0x44)});

    // ---- multiply / divide (HI/LO and the EE's second pair, MULT with rd)
    g.inst("mult", SPECIAL(4, 5, 0, 0, 0x18));
    g.inst("mult_rd", SPECIAL(4, 5, 2, 0, 0x18));
    g.inst("multu", SPECIAL(4, 5, 0, 0, 0x19));
    g.inst("div", SPECIAL(4, 5, 0, 0, 0x1A));
    g.inst("divu", SPECIAL(4, 5, 0, 0, 0x1B));
    g.inst("mult1", MMI(4, 5, 0, 0, 0x18));
    g.inst("mult1_rd", MMI(4, 5, 2, 0, 0x18));
    g.inst("multu1", MMI(4, 5, 0, 0, 0x19));
    g.inst("div1", MMI(4, 5, 0, 0, 0x1A));
    g.inst("divu1", MMI(4, 5, 0, 0, 0x1B));
    g.inst("madd", MMI(4, 5, 2, 0, 0x00));
    g.inst("maddu", MMI(4, 5, 2, 0, 0x01));
    g.inst("mfhi1", MMI(0, 0, 2, 0, 0x10));
    g.inst("mflo1", MMI(0, 0, 2, 0, 0x12));
    g.inst("mthi1", MMI(4, 0, 0, 0, 0x11));
    g.inst("mtlo1", MMI(4, 0, 0, 0, 0x13));
    g.inst("slt", SPECIAL(4, 5, 2, 0, 0x2A));
    g.inst("sltu", SPECIAL(4, 5, 2, 0, 0x2B));
    g.inst("dsra32", SPECIAL(0, 4, 2, 3, 0x3F));
    g.inst("dsll32", SPECIAL(0, 4, 2, 3, 0x3C));
    g.inst("dsrl32", SPECIAL(0, 4, 2, 3, 0x3E));
    g.inst("movn", SPECIAL(4, 5, 2, 0, 0x0B));
    g.inst("movz", SPECIAL(4, 5, 2, 0, 0x0A));
    g.inst("lui_ori", I(0x0F, 0, 2, 0x8000));
    g.inst("sra", SPECIAL(0, 4, 2, 4, 0x03));
    g.inst("srl", SPECIAL(0, 4, 2, 4, 0x02));
    g.inst("sllv", SPECIAL(5, 4, 2, 0, 0x04));
    g.inst("srav", SPECIAL(5, 4, 2, 0, 0x07));

    // ---- unaligned and quadword memory: address in a0, value in a1, destination v0 = $2 (old value
    //      of rt is preloaded by the test), offset 0 so the low address bits are the test's choice.
    g.inst("lwl", I(0x22, 4, 2, 0));
    g.inst("lwr", I(0x26, 4, 2, 0));
    g.inst("ldl", I(0x1A, 4, 2, 0));
    g.inst("ldr", I(0x1B, 4, 2, 0));
    g.inst("swl", I(0x2A, 4, 5, 0));
    g.inst("swr", I(0x2E, 4, 5, 0));
    g.inst("sdl", I(0x2C, 4, 5, 0));
    g.inst("sdr", I(0x2D, 4, 5, 0));
    g.inst("lb", I(0x20, 4, 2, 0));
    g.inst("lbu", I(0x24, 4, 2, 0));
    g.inst("lh", I(0x21, 4, 2, 0));
    g.inst("lhu", I(0x25, 4, 2, 0));
    g.inst("lw", I(0x23, 4, 2, 0));
    g.inst("lwu", I(0x27, 4, 2, 0));
    g.inst("ld", I(0x37, 4, 2, 0));
    g.inst("lq", I(0x1E, 4, 2, 0));
    g.inst("sq", I(0x1F, 4, 5, 0));
    g.inst("sd", I(0x3F, 4, 5, 0));

    // ---- COP1 (single): f0 / f1 operands, f2 result. FCR31 in ctx->fcr31.
    g.inst("fadd", COP1S(1, 0, 2, 0x00));
    g.inst("fsub", COP1S(1, 0, 2, 0x01));
    g.inst("fmul", COP1S(1, 0, 2, 0x02));
    g.inst("fdiv", COP1S(1, 0, 2, 0x03));
    g.inst("fsqrt", COP1S(1, 0, 2, 0x04));   // SQRT.S fd, ft (the operand is ft = f1)
    g.inst("fabs", COP1S(0, 0, 2, 0x05));
    g.inst("fmov", COP1S(0, 0, 2, 0x06));
    g.inst("fneg", COP1S(0, 0, 2, 0x07));
    g.inst("frsqrt", COP1S(1, 0, 2, 0x16));  // RSQRT.S fd, fs, ft = fs / sqrt(ft)
    g.inst("fcvt_w", COP1S(0, 0, 2, 0x24));
    g.inst("fcvt_s", COP1W(0, 0, 2, 0x20));
    g.inst("fceq", COP1S(1, 0, 0, 0x32));
    g.inst("fclt", COP1S(1, 0, 0, 0x34));
    g.inst("fcle", COP1S(1, 0, 0, 0x36));
    g.inst("fmax", COP1S(1, 0, 2, 0x28));
    g.inst("fmin", COP1S(1, 0, 2, 0x29));
    g.inst("fmadd", COP1S(1, 0, 2, 0x1C));   // MADD.S fd = ACC + fs*ft
    g.inst("fmsub", COP1S(1, 0, 2, 0x1D));
    g.inst("fmula", COP1S(1, 0, 0, 0x1A));
    g.inst("fmadda", COP1S(1, 0, 0, 0x1E));
    g.inst("fadda", COP1S(1, 0, 0, 0x18));
    g.inst("mtc1_a0", R(0x11, 0x04, 4, 0, 0, 0));  // mtc1 a0, f0
    g.inst("mfc1_v0", R(0x11, 0x00, 2, 0, 0, 0));  // mfc1 v0, f0
    g.inst("ctc1_a0", R(0x11, 0x06, 4, 31, 0, 0)); // ctc1 a0, fcr31
    g.inst("cfc1_v0", R(0x11, 0x02, 2, 31, 0, 0)); // cfc1 v0, fcr31

    // ---- MMI (the ones the game uses, plus their neighbours): rd = v0 ($2), rs = a0, rt = a1
    auto mmi = [&](const std::string &n, uint32_t fn, uint32_t sa) { g.inst(n, MMI(4, 5, 2, sa, fn)); };
    mmi("paddw", 0x08, 0x00);
    mmi("psubw", 0x08, 0x01);
    mmi("pmaxw", 0x08, 0x03);
    mmi("paddh", 0x08, 0x04);
    mmi("psubh", 0x08, 0x05);
    mmi("paddb", 0x08, 0x08);
    mmi("psubb", 0x08, 0x09);
    mmi("paddsw", 0x08, 0x10);
    mmi("psubsw", 0x08, 0x11);
    mmi("pextlw", 0x08, 0x12);
    mmi("ppacw", 0x08, 0x13);
    mmi("pextlh", 0x08, 0x16);
    mmi("pextlb", 0x08, 0x1A);
    mmi("pextuw", 0x28, 0x12);
    mmi("pcpyld", 0x09, 0x0E);
    mmi("pand", 0x09, 0x12);
    mmi("pxor", 0x09, 0x13);
    mmi("pcpyud", 0x29, 0x0E);
    mmi("por", 0x29, 0x12);
    mmi("pnor", 0x29, 0x13);
    g.inst("pcpyh", MMI(0, 5, 2, 0x1B, 0x29));
    g.inst("pexeh", MMI(0, 5, 2, 0x1A, 0x09));
    g.inst("prot3w", MMI(0, 5, 2, 0x1F, 0x09));
    g.inst("pexch", MMI(0, 5, 2, 0x1A, 0x29));
    g.inst("pexcw", MMI(0, 5, 2, 0x1E, 0x29));
    g.inst("pexew", MMI(0, 5, 2, 0x1E, 0x09));
    g.inst("prevh", MMI(0, 5, 2, 0x1B, 0x09));
    g.inst("pextlw_swap", MMI(5, 4, 2, 0x12, 0x08));

    // ---- VU0 macro mode. vf registers: fs = vf1, ft = vf2, fd = vf3 ; ACC in ctx->vu0_acc.
    auto vu = [&](const std::string &n, uint32_t dest, uint32_t fn) { g.inst(n, VU1(dest, 2, 1, 3, fn)); };
    for (uint32_t d : {0xF, 0x8, 0x5, 0x3})
    {
        char suffix[8];
        std::snprintf(suffix, sizeof suffix, "_%X", d);
        const std::string s = suffix;
        vu("vadd" + s, d, 0x28);
        vu("vsub" + s, d, 0x2C);
        vu("vmul" + s, d, 0x2A);
        vu("vmadd" + s, d, 0x29);
        vu("vmsub" + s, d, 0x2D);
        vu("vmaddx" + s, d, 0x08);
        vu("vmaddw" + s, d, 0x0B);
        vu("vmulx" + s, d, 0x18);
        vu("vmulw" + s, d, 0x1B);
        vu("vaddy" + s, d, 0x01);
        vu("vsubz" + s, d, 0x06);
        vu("vmax" + s, d, 0x2B);
        vu("vmini" + s, d, 0x2F);
        g.inst("vmula" + s, VU2(d, 2, 1, 0x2A));
        g.inst("vmulax" + s, VU2(d, 2, 1, 0x18));
        g.inst("vmadda" + s, VU2(d, 2, 1, 0x29));
        g.inst("vmaddax" + s, VU2(d, 2, 1, 0x08));
        g.inst("vmadday" + s, VU2(d, 2, 1, 0x09));
        g.inst("vmaddaz" + s, VU2(d, 2, 1, 0x0A));
        g.inst("vmaddaw" + s, VU2(d, 2, 1, 0x0B));
        g.inst("vadda" + s, VU2(d, 2, 1, 0x28));
        g.inst("vsuba" + s, VU2(d, 2, 1, 0x2C));
        g.inst("vmove" + s, VU2(d, 2, 1, 0x30));
        g.inst("vmr32" + s, VU2(d, 2, 1, 0x31));
        g.inst("vabs" + s, VU2(d, 2, 1, 0x1D));
        g.inst("vitof4" + s, VU2(d, 2, 1, 0x11));
        g.inst("vitof12" + s, VU2(d, 2, 1, 0x12));
        g.inst("vftoi4" + s, VU2(d, 2, 1, 0x15));
        g.inst("vftoi0" + s, VU2(d, 2, 1, 0x14));
    }
    g.inst("vopmsub", VU1(0xE, 2, 1, 3, 0x2E));
    g.inst("vopmula", VU2(0xE, 2, 1, 0x2E));
    // VDIV Q, vf1.fsf, vf2.ftf with fsf = bits 21..22, ftf = bits 23..24 (dest field of the word).
    for (uint32_t fsf = 0; fsf < 4; ++fsf)
        for (uint32_t ftf = 0; ftf < 4; ++ftf)
        {
            const uint32_t dest = (ftf << 2) | fsf; // bits 21-22 = fsf, 23-24 = ftf
            char n[32];
            std::snprintf(n, sizeof n, "vdiv_%u_%u", fsf, ftf);
            g.inst(n, VU2(dest, 2, 1, 0x38));
            std::snprintf(n, sizeof n, "vsqrt_%u_%u", fsf, ftf);
            g.inst(n, VU2(dest, 2, 1, 0x39));
            std::snprintf(n, sizeof n, "vrsqrt_%u_%u", fsf, ftf);
            g.inst(n, VU2(dest, 2, 1, 0x3A));
        }
    g.inst("qmtc2_vf1", R(0x12, 0x05, 4, 1, 0, 0)); // qmtc2 a0, vf1
    g.inst("qmfc2_v0", R(0x12, 0x01, 2, 1, 0, 0));  // qmfc2 v0, vf1
    g.inst("cfc2_status", R(0x12, 0x02, 2, 16, 0, 0));
    g.inst("cfc2_mac", R(0x12, 0x02, 2, 17, 0, 0));
    g.inst("ctc2_status", R(0x12, 0x06, 4, 16, 0, 0));

    std::ofstream f(argv[1]);
    f << "// GENERATED by ee_semantics_gen from the recompiler's own translators. Do not edit.\n";
    f << g.out.str();
    f << "struct GenCase { const char *name; void (*fn)(uint8_t *, R5900Context *, PS2Runtime *); };\n";
    f << "static const GenCase kInstCases[] = {\n";
    for (const auto &n : g.names)
        f << "    {\"" << n << "\", c_" << n << "},\n";
    f << "};\n";
    f << "static const GenCase kProgramCases[] = {\n";
    for (const auto &n : g.programs)
        f << "    {\"" << n << "\", p_" << n << "},\n";
    f << "};\n";
    return 0;
}
