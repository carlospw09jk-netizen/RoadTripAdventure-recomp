#!/bin/zsh
# Variant of the new (cpu-accuracy) build whose DIV.S, SQRT.S and RSQRT.S round to nearest, the rest
# toward zero (PCSX2's defaults: FPUDiv.Roundmode = Nearest, FPU.Roundmode = Chop). The helpers
# ps2_fpu_div_s / sqrt_s / rsqrt_s in the SDK header are patched to switch the FPCR rounding mode
# for the one instruction (aarch64 only). Game code compiled by the app picks the header up.
# usage: mkdivrn.sh <new .app> <out dir>
NEW=$1; OUT=$2; VU=${3:-}   # third argument "vu": the VU0 macro DIV/SQRT/RSQRT round to nearest too
RES=Contents/Resources
APP=$OUT/divrn$VU.app
rm -rf $APP; cp -R $NEW $APP
python3 - $APP/$RES/sdk/include/0/ps2_runtime_macros.h $VU <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
helpers = r'''
// A/B: DIV.S, SQRT.S, RSQRT.S round to nearest (the FPCR's other rounding stays toward zero).
static inline float ps2_div_rn(float a, float b)
{
    uint64_t o, n; float r;
    __asm__ volatile("mrs %0, fpcr\n\tbic %1, %0, #(3<<22)\n\tmsr fpcr, %1\n\tfdiv %s2, %s3, %s4\n\tmsr fpcr, %0"
                     : "=&r"(o), "=&r"(n), "=&w"(r) : "w"(a), "w"(b) : "memory");
    return r;
}
static inline float ps2_sqrt_rn(float a)
{
    uint64_t o, n; float r;
    __asm__ volatile("mrs %0, fpcr\n\tbic %1, %0, #(3<<22)\n\tmsr fpcr, %1\n\tfsqrt %s2, %s3\n\tmsr fpcr, %0"
                     : "=&r"(o), "=&r"(n), "=&w"(r) : "w"(a) : "memory");
    return r;
}
'''
marker = "// EE FPU semantics that differ from IEEE"
assert marker in s
s = s.replace(marker, helpers + "\n" + marker, 1)
a = "    return static_cast<float>(std::sqrt(std::fabs(static_cast<double>(ft))));\n}\n\n// RSQRT.S"
assert a in s
s = s.replace(a, "    return ps2_sqrt_rn(std::fabs(ft));\n}\n\n// RSQRT.S", 1)
b = "    const float root = static_cast<float>(std::sqrt(std::fabs(static_cast<double>(ft))));\n    return fs / root;"
assert b in s
s = s.replace(b, "    const float root = ps2_sqrt_rn(std::fabs(ft));\n    return ps2_div_rn(fs, root);", 1)
c = "        return ps2_fpu::fromBits(((bs ^ bt) & 0x80000000u) | 0x7F7FFFFFu);\n    }\n    return fs / ft;\n}\n\n// VU0"
assert c in s
s = s.replace(c, c.replace("return fs / ft;", "return ps2_div_rn(fs, ft);"), 1)
mode = sys.argv[2] if len(sys.argv) > 2 else ""
if "cvt" in mode:   # CVT.S.W rounds to nearest too
    cv = "#define FPU_CVT_S_W(a) ((float)(int32_t)(a))"
    assert cv in s
    helper = r'''static inline float ps2_cvt_rn(int32_t a)
{
    uint64_t o, n; float r;
    __asm__ volatile("mrs %0, fpcr\n\tbic %1, %0, #(3<<22)\n\tmsr fpcr, %1\n\tscvtf %s2, %w3\n\tmsr fpcr, %0"
                     : "=&r"(o), "=&r"(n), "=&w"(r) : "r"(a) : "memory");
    return r;
}
'''
    s = s.replace(cv, "#define FPU_CVT_S_W(a) ps2_cvt_rn((int32_t)(a))", 1)
    s = s.replace("// EE FPU semantics that differ from IEEE", helper + "// EE FPU semantics that differ from IEEE", 1)
if "vu" in mode:
    d = "    return fs / ft;\n}\n\ninline float ps2_vu_sqrt"
    assert d in s
    s = s.replace(d, d.replace("return fs / ft;", "return ps2_div_rn(fs, ft);"), 1)
    q = "inline float ps2_vu_sqrt(float ft)\n{\n    return static_cast<float>(std::sqrt(std::fabs(static_cast<double>(ft))));\n}"
    assert q in s
    s = s.replace(q, "inline float ps2_vu_sqrt(float ft)\n{\n    return ps2_sqrt_rn(std::fabs(ft));\n}", 1)
    r = "    return fs / ps2_vu_sqrt(ft);"
    assert r in s
    s = s.replace(r, "    return ps2_div_rn(fs, ps2_vu_sqrt(ft));", 1)
open(p, "w").write(s)
EOF
echo "div-rn" > $APP/$RES/sdk/build_id
codesign --force --deep --sign - $APP >/dev/null 2>&1
echo made $APP
